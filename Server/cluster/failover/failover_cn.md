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

![elect](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/failover/elect.png)

并且把 `FAIL` 广播给集群中所有的节点

![elect2](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/failover/elect2.png)

Every slave will call `clusterHandleSlaveFailover` in each loop of `clusterCron`

    void clusterHandleSlaveFailover(void) {
        mstime_t data_age;
        mstime_t auth_age = mstime() - server.cluster->failover_auth_time;
        int needed_quorum = (server.cluster->size / 2) + 1;
        int manual_failover = server.cluster->mf_end != 0 &&
                              server.cluster->mf_can_start;
        mstime_t auth_timeout, auth_retry_time;

        server.cluster->todo_before_sleep &= ~CLUSTER_TODO_HANDLE_FAILOVER;

        /* Compute the failover timeout (the max time we have to send votes
         * and wait for replies), and the failover retry time (the time to wait
         * before trying to get voted again).
         *
         * Timeout is MAX(NODE_TIMEOUT*2,2000) milliseconds.
         * Retry is two times the Timeout.
         */
        auth_timeout = server.cluster_node_timeout*2;
        if (auth_timeout < 2000) auth_timeout = 2000;
        auth_retry_time = auth_timeout*2;

        /* Pre conditions to run the function, that must be met both in case
         * of an automatic or manual failover:
         * 1) We are a slave.
         * 2) Our master is flagged as FAIL, or this is a manual failover.
         * 3) We don't have the no failover configuration set, and this is
         *    not a manual failover.
         * 4) It is serving slots. */
        if (nodeIsMaster(myself) ||
            myself->slaveof == NULL ||
            (!nodeFailed(myself->slaveof) && !manual_failover) ||
            (server.cluster_slave_no_failover && !manual_failover) ||
            myself->slaveof->numslots == 0)
        {
            /* There are no reasons to failover, so we set the reason why we
             * are returning without failing over to NONE. */
            server.cluster->cant_failover_reason = CLUSTER_CANT_FAILOVER_NONE;
            return;
        }
        /* ... */
        /* Ask for votes if needed. */
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
            return; /* Wait for replies. */
        }

        /* Check if we reached the quorum. */
        if (server.cluster->failover_auth_count >= needed_quorum) {
            /* We have the quorum, we can finally failover the master. */

            serverLog(LL_WARNING,
                "Failover election won: I'm the new master.");

            /* Update my configEpoch to the epoch of the election. */
            if (myself->configEpoch < server.cluster->failover_auth_epoch) {
                myself->configEpoch = server.cluster->failover_auth_epoch;
                serverLog(LL_WARNING,
                    "configEpoch set to %llu after successful failover",
                    (unsigned long long) myself->configEpoch);
            }

            /* Take responsibility for the cluster slots. */
            clusterFailoverReplaceYourMaster();
        } else {
            clusterLogCantFailover(CLUSTER_CANT_FAILOVER_WAITING_VOTES);
        }
    }

If our master is in `FAIL` state, our epoch number will be added, and we will ask every other nodes to vote us, but only master serving at least one slot has the right to vote

![elect3](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/failover/elect3.png)

And the master will vote only if meet the following requirements

    /* Vote for the node asking for our vote if there are the conditions. */
    void clusterSendFailoverAuthIfNeeded(clusterNode *node, clusterMsg *request) {
        clusterNode *master = node->slaveof;
        uint64_t requestCurrentEpoch = ntohu64(request->currentEpoch);
        uint64_t requestConfigEpoch = ntohu64(request->configEpoch);
        unsigned char *claimed_slots = request->myslots;
        int force_ack = request->mflags[0] & CLUSTERMSG_FLAG0_FORCEACK;
        int j;

        /* IF we are not a master serving at least 1 slot, we don't have the
         * right to vote, as the cluster size in Redis Cluster is the number
         * of masters serving at least one slot, and quorum is the cluster
         * size + 1 */
        if (nodeIsSlave(myself) || myself->numslots == 0) return;

        /* Request epoch must be >= our currentEpoch.
         * Note that it is impossible for it to actually be greater since
         * our currentEpoch was updated as a side effect of receiving this
         * request, if the request epoch was greater. */
        if (requestCurrentEpoch < server.cluster->currentEpoch) {
            serverLog(LL_WARNING,
                "Failover auth denied to %.40s: reqEpoch (%llu) < curEpoch(%llu)",
                node->name,
                (unsigned long long) requestCurrentEpoch,
                (unsigned long long) server.cluster->currentEpoch);
            return;
        }

        /* I already voted for this epoch? Return ASAP. */
        if (server.cluster->lastVoteEpoch == server.cluster->currentEpoch) {
            serverLog(LL_WARNING,
                    "Failover auth denied to %.40s: already voted for epoch %llu",
                    node->name,
                    (unsigned long long) server.cluster->currentEpoch);
            return;
        }

        /* Node must be a slave and its master down.
         * The master can be non failing if the request is flagged
         * with CLUSTERMSG_FLAG0_FORCEACK (manual failover). */
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

        /* We did not voted for a slave about this master for two
         * times the node timeout. This is not strictly needed for correctness
         * of the algorithm but makes the base case more linear. */
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

        /* The slave requesting the vote must have a configEpoch for the claimed
         * slots that is >= the one of the masters currently serving the same
         * slots in the current configuration. */
        for (j = 0; j < CLUSTER_SLOTS; j++) {
            if (bitmapTestBit(claimed_slots, j) == 0) continue;
            if (server.cluster->slots[j] == NULL ||
                server.cluster->slots[j]->configEpoch <= requestConfigEpoch)
            {
                continue;
            }
            /* If we reached this point we found a slot that in our current slots
             * is served by a master with a greater configEpoch than the one claimed
             * by the slave requesting our vote. Refuse to vote for this slave. */
            serverLog(LL_WARNING,
                    "Failover auth denied to %.40s: "
                    "slot %d epoch (%llu) > reqEpoch (%llu)",
                    node->name, j,
                    (unsigned long long) server.cluster->slots[j]->configEpoch,
                    (unsigned long long) requestConfigEpoch);
            return;
        }

        /* We can vote for this slave. */
        server.cluster->lastVoteEpoch = server.cluster->currentEpoch;
        node->slaveof->voted_time = mstime();
        clusterDoBeforeSleep(CLUSTER_TODO_SAVE_CONFIG|CLUSTER_TODO_FSYNC_CONFIG);
        clusterSendFailoverAuth(node);
        serverLog(LL_WARNING, "Failover auth granted to %.40s for epoch %llu",
            node->name, (unsigned long long) server.cluster->currentEpoch);
    }

Once the slave gets majority of votes, it will become master

![elect4](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/failover/elect4.png)

And because the master now has a higher epoch number, if the old master rejoin the cluster, it will follows the configuration of those with higher epoch number, and becomes the slave of the current master

![elect5](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/failover/elect5.png)

The situations in the real world is more complicated, such as multiply slaves and partition, you need to search for more information such as [raft paper](https://raft.github.io/raft.pdf)
