# failover

# contents

* [需要提前了解的知识](#需要提前了解的知识)
* [PFAIL](#PFAIL)
* [FAIL](#FAIL)
* [ELECT](#ELECT)

# 需要提前了解的知识

* [cluster->reshard](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/cluster_cn.md#reshard)
* [cluster->gossip](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/gossip/gossip_cn.md)

[prerequisites->cluster](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/cluster_cn.md) 中展示了示例集群的配置

如果我们增加一个新的节点 `127.0.0.1:7003`

    127.0.0.1:7003> CLUSTER MEET 127.0.0.1 7000
    OK
    127.0.0.1:7003> CLUSTER NODES
    75ddecf9169096eeff03ec60f868859333c824a1 127.0.0.1:7003@17003 myself,master - 0 1579513065000 0 connected
    ba060347b647e2cb050eea666f087fd08d4e42d7 127.0.0.1:7001@17001 master - 0 1579513065256 2 connected 5461-10922
    c62cfa7c26ec449462cceb5fd45cac121d866d44 127.0.0.1:7002@17002 master - 0 1579513066283 3 connected 10923-16383
    89c21152eedde6d9dec2fcebf4330b39810bc179 127.0.0.1:7000@17000 master - 0 1579513066797 1 connected 0-5460
    127.0.0.1:7003> CLUSTER REPLICATE 89c21152eedde6d9dec2fcebf4330b39810bc179

并把它设置为节点 `127.0.0.1:7000` 的从节点

    127.0.0.1:7003> CLUSTER REPLICATE 89c21152eedde6d9dec2fcebf4330b39810bc179
    OK
    127.0.0.1:7003> CLUSTER NODES
    75ddecf9169096eeff03ec60f868859333c824a1 127.0.0.1:7003@17003 myself,slave 89c21152eedde6d9dec2fcebf4330b39810bc179 0 1579513200000 0 connected
    ba060347b647e2cb050eea666f087fd08d4e42d7 127.0.0.1:7001@17001 master - 0 1579513201517 2 connected 5461-10922
    c62cfa7c26ec449462cceb5fd45cac121d866d44 127.0.0.1:7002@17002 master - 0 1579513201000 3 connected 10923-16383
    89c21152eedde6d9dec2fcebf4330b39810bc179 127.0.0.1:7000@17000 master - 0 1579513201833 1 connected 0-5460


并且让节点 `127.0.0.1:7000` 宕机

    ^C943:signal-handler (1579016631) Received SIGINT scheduling shutdown...
    943:M 14 Jan 2020 23:43:51.330 # User requested shutdown...
    943:M 14 Jan 2020 23:43:51.330 * Calling fsync() on the AOF file.
    943:M 14 Jan 2020 23:43:51.330 # Redis is now ready to exit, bye bye...
    zpoint@bogon 7000 %

我们可以发现节点 `127.0.0.1:7003` 变成了主节点并且该节点的 epoch 值成了集群中最大的值

    127.0.0.1:7003> CLUSTER NODES
    ba060347b647e2cb050eea666f087fd08d4e42d7 127.0.0.1:7001@17001 master - 0 1579572427408 2 connected 5461-10922
    89c21152eedde6d9dec2fcebf4330b39810bc179 127.0.0.1:7000@17000 master,fail - 1579572351865 1579572351452 5 disconnected
    75ddecf9169096eeff03ec60f868859333c824a1 127.0.0.1:7003@17003 myself,master - 0 1579572426000 7 connected 0-5460
    c62cfa7c26ec449462cceb5fd45cac121d866d44 127.0.0.1:7002@17002 master - 0 1579572426898 3 connected 10923-16383


# PFAIL

我们让节点 `127.0.0.1:7000` 宕机时发生了什么 ?

![overview](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/failover/overview.png)

对于 `master/7001`, `master/7002` 还有 `slave/7003`, `clusterCron` 会在超过 `cluster-node-timeout` 毫秒后还无法接收到 PONG 消息的节点标记成 `PFAIL`, 默认值为 15 秒

      /* 计算 PONG 的延时, 注意如果我们已经收到了 PONG 消息, 则 node->ping_sent 会被置为 0, 所以代码进不到这里 */
        delay = now - node->ping_sent;

    if (delay > server.cluster_node_timeout) {
        /* 达到超时条件, 如果这个节点不在 PFAIL 状态, 则把这个节点置于 PFAIL 状态 */
        if (!(node->flags & (CLUSTER_NODE_PFAIL|CLUSTER_NODE_FAIL))) {
            serverLog(LL_DEBUG,"*** NODE %.40s possibly failing",
                node->name);
            node->flags |= CLUSTER_NODE_PFAIL;
            update_state = 1;
        }
    }

![pfail](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/failover/pfail.png)

`master/7001`, `master/7002` 和 `slave/7003` 会通过 [gossip](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/gossip/gossip_cn.md) 协议进行交流, 在 `cluster-node-timeout` 毫秒之后还收不到某个节点的 `PONG` 消息时, 就会把这个节点置于 PFAIL` 状态

![pfail2](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/failover/pfail2.png)

每一个节点都有自己的 `cluster-node-timeout` 配置, 并且每个节点发送 `PING` 消息的时刻都不相同, 但如果节点 `master/7000` 真的宕机了, 在不同节点的 `PING 时刻` + `cluster-node-timeout` 毫秒之后, 大部分的正常节点都会把 `master/7000` 标记为 `PFAIL` 状态

# FAIL

`master/7000` 的 `PFAIL` 状态会通过 [gossip](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/gossip/gossip_cn.md) 协议传播, 每当一个节点通过该协议接收到集群中的一个节点处于 `PFAIL` 状态时, `markNodeAsFailingIfNeeded` 这个函数就会被调用

`markNodeAsFailingIfNeeded` 会在至少有 `(server.cluster->size / 2) + 1` 个节点把对应节点标记成 `PFAIL` 状态时就把这个节点直接置为 `FAIL` 状态, 既在当前集群收到 2 个关于 节点 `master/7000` 的 `PFAIL` 状态, 就把这个节点设置为 `FAIL` 状态

    void markNodeAsFailingIfNeeded(clusterNode *node) {
        int failures;
        int needed_quorum = (server.cluster->size / 2) + 1;

        if (!nodeTimedOut(node)) return; /* 我自己能和目标节点收发消息 */
        if (nodeFailed(node)) return; /* 这个节点已经被标记成 FAIL 了 */

        failures = clusterNodeFailureReportsCount(node);
        /* 如果我自己也是 master, 那么把我自己也计入数目中 */
        if (nodeIsMaster(myself)) failures++;
        if (failures < needed_quorum) return; /* 把目标节点标记成 PFAIL 的 master 节点还不集群数目的一半, 不操作 */

        serverLog(LL_NOTICE,
            "Marking node %.40s as failing (quorum reached).", node->name);

        /* 把目标节点标记成 FAIL */
        node->flags &= ~CLUSTER_NODE_PFAIL;
        node->flags |= CLUSTER_NODE_FAIL;
        node->fail_time = mstime();

        /* 把目标节点被标记成 FAIL 这个信息广播给集群里的所有其他节点,
         * 让所有能与我自己建立连接的节点都把目标节点标记成 FAIL */
        if (nodeIsMaster(myself)) clusterSendFail(node->name);
        clusterDoBeforeSleep(CLUSTER_TODO_UPDATE_STATE|CLUSTER_TODO_SAVE_CONFIG);
    }

如果 `master/7001` 是第一个发现 `master/7000` 有 2 个 `PFAIL` 状态的话, 它会把它标记成 `FAIL` 状态并在集群中广播该条状态信息

![fail](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/failover/fail.png)

# ELECT

只要一个节点获得了 `master/7000` 的 2 个 `PFAIL` 状态, 那么这个节点就会把 `master/7000` 标记为 `FAIL`

只有槽数大于 1 的 master 节点的 `PFAIL` 算数

![elect](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/failover/elect.png)

并且把这个节点的 `FAIL` 状态广播给集群中所有的节点

![elect2](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/failover/elect2.png)

每一个 slave 在每一次 `clusterCron` 循环中都会调用 `clusterHandleSlaveFailover` 这个函数

    void clusterHandleSlaveFailover(void) {
        mstime_t data_age;
        mstime_t auth_age = mstime() - server.cluster->failover_auth_time;
        int needed_quorum = (server.cluster->size / 2) + 1;
        int manual_failover = server.cluster->mf_end != 0 &&
                              server.cluster->mf_can_start;
        mstime_t auth_timeout, auth_retry_time;

        server.cluster->todo_before_sleep &= ~CLUSTER_TODO_HANDLE_FAILOVER;

        /* 计算 failover 超时 (发送投票消息和等待响应的最大时间)
         * 和 failover 的重试时间 (再次发起投票请求的时间)
         * 超时是 MAX(NODE_TIMEOUT*2,2000) 毫秒
         * 重试时间是 上面的超时 * 2
         */
        auth_timeout = server.cluster_node_timeout*2;
        if (auth_timeout < 2000) auth_timeout = 2000;
        auth_retry_time = auth_timeout*2;

        /* 不论是手动的 failover 还是自动的 failover
         * 运行该函数需要满足下面所有的先决条件
         * 1) 我自己本身是 slave
         * 2) 我的 master 已经被标记成 FAIL 状态, 或者这次是一个手动的 failover
         * 3) 设置里没有配置不允许 failover 的选项, 并且本次不是一个手动的 failover
         * 4）当前节点没有服务任何一个槽
        if (nodeIsMaster(myself) ||
            myself->slaveof == NULL ||
            (!nodeFailed(myself->slaveof) && !manual_failover) ||
            (server.cluster_slave_no_failover && !manual_failover) ||
            myself->slaveof->numslots == 0)
        {
            /* 没有必要进行 failover, 我们设置了为什么不能进行 failover 的原因并返回 */
            server.cluster->cant_failover_reason = CLUSTER_CANT_FAILOVER_NONE;
            return;
        }
        /* ... */
        /* 必要的时候发起新的一轮投票 */
        if (server.cluster->failover_auth_sent == 0) {
            server.cluster->currentEpoch++;
            server.cluster->failover_auth_epoch = server.cluster->currentEpoch;
            serverLog(LL_WARNING,"Starting a failover election for epoch %llu.",
                (unsigned long long) server.cluster->currentEpoch);
            clusterRequestFailoverAuth();
            server.cluster->failover_auth_sent = 1;
            clusterDoBeforeSleep(CLUSTER_TODO_SAVE_CONFIG|
                                 CLUSTER_TODO_UPDATE_STATE|
                                 CLUSTER_TODO_FSYNC_CONFIG);
            return; /* 等待回复 */
        }

        /* 查看是否有法定的投票数投了我 */
        if (server.cluster->failover_auth_count >= needed_quorum) {
            /* 我们有达到执行的票数, 可以进行 failover */

            serverLog(LL_WARNING,
                "Failover election won: I'm the new master.");

            /* 把自己的 configEpoch 设置为选举的 epoch */
            if (myself->configEpoch < server.cluster->failover_auth_epoch) {
                myself->configEpoch = server.cluster->failover_auth_epoch;
                serverLog(LL_WARNING,
                    "configEpoch set to %llu after successful failover",
                    (unsigned long long) myself->configEpoch);
            }

            /* 执行替换 master 槽等一系列的操作 */
            clusterFailoverReplaceYourMaster();
        } else {
            clusterLogCantFailover(CLUSTER_CANT_FAILOVER_WAITING_VOTES);
        }
    }

如果我们的 master 在 `FAIL` 状态下, 我们的 epoch 会加一, 并且要求所有其他的至少服务一个槽的 master 节点对我自己进行投票

![elect3](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/failover/elect3.png)

并且 master 只会在符合以下条件时进行投票

    /* 对于要求给他进行投票的节点, 符合以下条时进行投票 */
    void clusterSendFailoverAuthIfNeeded(clusterNode *node, clusterMsg *request) {
        clusterNode *master = node->slaveof;
        uint64_t requestCurrentEpoch = ntohu64(request->currentEpoch);
        uint64_t requestConfigEpoch = ntohu64(request->configEpoch);
        unsigned char *claimed_slots = request->myslots;
        int force_ack = request->mflags[0] & CLUSTERMSG_FLAG0_FORCEACK;
        int j;

        /* 如果我们不是一个至少服务一个槽的节点, 我们是没有权利进行投票的
         * 因为集群的大小为 至少服务一个槽的节点的数量, 并且合法投票数的阈值是 集群大小 + 1 */
        if (nodeIsSlave(myself) || myself->numslots == 0) return;

        /* 要求投票的 epoch 数必须大于等于我们自身的 epoch 数
         * 因为只要请求的 epoch 数比我们自身的 epoch 数大, 我们自己的 epoch 数就会置为请求的 epoch 数, 所以在请求之后实际上请求的 epoch 是不会大于我们自身的 epoch 的 */
        if (requestCurrentEpoch < server.cluster->currentEpoch) {
            serverLog(LL_WARNING,
                "Failover auth denied to %.40s: reqEpoch (%llu) < curEpoch(%llu)",
                node->name,
                (unsigned long long) requestCurrentEpoch,
                (unsigned long long) server.cluster->currentEpoch);
            return;
        }

        /* 我已经对这一轮 epoch 进行过投票了 ? 马上返回 */
        if (server.cluster->lastVoteEpoch == server.cluster->currentEpoch) {
            serverLog(LL_WARNING,
                    "Failover auth denied to %.40s: already voted for epoch %llu",
                    node->name,
                    (unsigned long long) server.cluster->currentEpoch);
            return;
        }

        /* 请求投票的节点必须是一个 slave 并且它的 master 当前处于无法连接的状态
         * 但是请求带有 CLUSTERMSG_FLAG0_FORCEACK 标记时可以例外
         * /
        if (nodeIsMaster(node) || master == NULL ||
            (!nodeFailed(master) && !force_ack))
        {
            if (nodeIsMaster(node)) {
                serverLog(LL_WARNING,
                        "Failover auth denied to %.40s: it is a master node",
                        node->name);
            } else if (master == NULL) {
                serverLog(LL_WARNING,
                        "Failover auth denied to %.40s: I don't know its master",
                        node->name);
            } else if (!nodeFailed(master)) {
                serverLog(LL_WARNING,
                        "Failover auth denied to %.40s: its master is up",
                        node->name);
            }
            return;
        }

        /* 我们在 cluster_node_timeout * 2 的延时内是不会对对应节点的 slave 进行投票的
         * 这不是为了算法的准确性而做的设计, 而是为了表现得更平稳一些 */
        if (mstime() - node->slaveof->voted_time < server.cluster_node_timeout * 2)
        {
            serverLog(LL_WARNING,
                    "Failover auth denied to %.40s: "
                    "can't vote about this master before %lld milliseconds",
                    node->name,
                    (long long) ((server.cluster_node_timeout*2)-
                                 (mstime() - node->slaveof->voted_time)));
            return;
        }

        /* 要求投票的 slave 必须拥有 >= 他自己的 master 的槽数(在当前节点的 epoch) */
        for (j = 0; j < CLUSTER_SLOTS; j++) {
            if (bitmapTestBit(claimed_slots, j) == 0) continue;
            if (server.cluster->slots[j] == NULL ||
                server.cluster->slots[j]->configEpoch <= requestConfigEpoch)
            {
                continue;
            }
            /* 进到这里, 我们发现了至少一个我们在服务的槽, 同时也在被要求投票的节点的 master 服务着
             * 并且这个 master 的 epoch 大于请求的 epoch, 我们需要拒绝本次投票 */
            serverLog(LL_WARNING,
                    "Failover auth denied to %.40s: "
                    "slot %d epoch (%llu) > reqEpoch (%llu)",
                    node->name, j,
                    (unsigned long long) server.cluster->slots[j]->configEpoch,
                    (unsigned long long) requestConfigEpoch);
            return;
        }

        /* 我们可以对这个 slave 进行投票 */
        server.cluster->lastVoteEpoch = server.cluster->currentEpoch;
        node->slaveof->voted_time = mstime();
        clusterDoBeforeSleep(CLUSTER_TODO_SAVE_CONFIG|CLUSTER_TODO_FSYNC_CONFIG);
        clusterSendFailoverAuth(node);
        serverLog(LL_WARNING, "Failover auth granted to %.40s for epoch %llu",
            node->name, (unsigned long long) server.cluster->currentEpoch);
    }

只要这个 slave 获得了集群中超过一半的有效投票, 那么它就会变成 master

![elect4](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/failover/elect4.png)

并且因为这个 master 当前有更高的 epoch 数, 如果之前宕机的 master 此时重新恢复并加入集群, 他会根据更高的 epoch 的节点的配置更新, 并且变成新的 master 的 slave

![elect5](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/failover/elect5.png)

真实世界的情况会更加复杂, 比如多个从节点的投票竞争, 或者网络分区, 课可以查阅更多资料了解相关信息, 比如 [raft 论文](https://raft.github.io/raft.pdf)
