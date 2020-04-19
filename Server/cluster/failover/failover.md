# failover

# contents

* [prerequisites](#prerequisites)
* [PFAIL](#PFAIL)
* [FAIL](#FAIL)
* [ELECT](#ELECT)

# prerequisites

* [cluster->reshard](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/cluster.md#reshard)
* [cluster->gossip](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/gossip/gossip.md)

The cluster example is set up in [prerequisites->cluster](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/cluster.md)

If we add a new node `127.0.0.1:7003`

```shell script
127.0.0.1:7003> CLUSTER MEET 127.0.0.1 7000
OK
127.0.0.1:7003> CLUSTER NODES
75ddecf9169096eeff03ec60f868859333c824a1 127.0.0.1:7003@17003 myself,master - 0 1579513065000 0 connected
ba060347b647e2cb050eea666f087fd08d4e42d7 127.0.0.1:7001@17001 master - 0 1579513065256 2 connected 5461-10922
c62cfa7c26ec449462cceb5fd45cac121d866d44 127.0.0.1:7002@17002 master - 0 1579513066283 3 connected 10923-16383
89c21152eedde6d9dec2fcebf4330b39810bc179 127.0.0.1:7000@17000 master - 0 1579513066797 1 connected 0-5460
127.0.0.1:7003> CLUSTER REPLICATE 89c21152eedde6d9dec2fcebf4330b39810bc179

```

And set it as a slave of node `127.0.0.1:7000`

```shell script
127.0.0.1:7003> CLUSTER REPLICATE 89c21152eedde6d9dec2fcebf4330b39810bc179
OK
127.0.0.1:7003> CLUSTER NODES
75ddecf9169096eeff03ec60f868859333c824a1 127.0.0.1:7003@17003 myself,slave 89c21152eedde6d9dec2fcebf4330b39810bc179 0 1579513200000 0 connected
ba060347b647e2cb050eea666f087fd08d4e42d7 127.0.0.1:7001@17001 master - 0 1579513201517 2 connected 5461-10922
c62cfa7c26ec449462cceb5fd45cac121d866d44 127.0.0.1:7002@17002 master - 0 1579513201000 3 connected 10923-16383
89c21152eedde6d9dec2fcebf4330b39810bc179 127.0.0.1:7000@17000 master - 0 1579513201833 1 connected 0-5460


```

If we fail the node `127.0.0.1:7000`

```c
^C943:signal-handler (1579016631) Received SIGINT scheduling shutdown...
943:M 14 Jan 2020 23:43:51.330 # User requested shutdown...
943:M 14 Jan 2020 23:43:51.330 * Calling fsync() on the AOF file.
943:M 14 Jan 2020 23:43:51.330 # Redis is now ready to exit, bye bye...
zpoint@bogon 7000 %

```

We can find that the node `127.0.0.1:7003` becomes master and the epoch num is the greatest

```shell script
127.0.0.1:7003> CLUSTER NODES
ba060347b647e2cb050eea666f087fd08d4e42d7 127.0.0.1:7001@17001 master - 0 1579572427408 2 connected 5461-10922
89c21152eedde6d9dec2fcebf4330b39810bc179 127.0.0.1:7000@17000 master,fail - 1579572351865 1579572351452 5 disconnected
75ddecf9169096eeff03ec60f868859333c824a1 127.0.0.1:7003@17003 myself,master - 0 1579572426000 7 connected 0-5460
c62cfa7c26ec449462cceb5fd45cac121d866d44 127.0.0.1:7002@17002 master - 0 1579572426898 3 connected 10923-16383


```

# PFAIL

What happended when we fail the node `127.0.0.1:7000` ?

![overview](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/failover/overview.png)

For `master/7001`, `master/7002` and `slave/7003`

`clusterCron` will mark the node as `PFAIL` state if the current node can't receive the `PONG` message for the target node in `cluster-node-timeout` millseconds, default value is 15s

```c
  /* Compute the delay of the PONG. Note that if we already received
   * the PONG, then node->ping_sent is zero, so can't reach this code at all. */
    delay = now - node->ping_sent;

if (delay > server.cluster_node_timeout) {
    /* Timeout reached. Set the node as possibly failing if it is
     * not already in this state. */
    if (!(node->flags & (CLUSTER_NODE_PFAIL|CLUSTER_NODE_FAIL))) {
        serverLog(LL_DEBUG,"*** NODE %.40s possibly failing",
            node->name);
        node->flags |= CLUSTER_NODE_PFAIL;
        update_state = 1;
    }
}

```

![pfail](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/failover/pfail.png)

`master/7001`, `master/7002` and `slave/7003` will communicate with `master/7000` via [gossip](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/gossip/gossip.md) protocol, After `cluster-node-timeout` millseconds they are not able to receive the `PONG` message, each of them will mark `master/7000` as `PFAIL` state

![pfail2](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/failover/pfail2.png)

Every node will has it's own `cluster-node-timeout` configuration, and the time they send `PING` message will various, but if the node `master/7000` really fails, after `timestamp in PING` + `cluster-node-timeout`, majority of node will mark `master/7000` as `PFAIL`

# FAIL

The `PFAIL` state of the `master/7000` will propagate via the [gossip](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/gossip/gossip.md) protocol, whenever a node receive a node with `PFAIL` state, `markNodeAsFailingIfNeeded` will be called

`markNodeAsFailingIfNeeded` will mark the node `master/7000` as `FAIL` state if there're at least `(server.cluster->size / 2) + 1` says the node is in `PFAIL` state, which is 2 in the current cluster

```c
void markNodeAsFailingIfNeeded(clusterNode *node) {
    int failures;
    int needed_quorum = (server.cluster->size / 2) + 1;

    if (!nodeTimedOut(node)) return; /* We can reach it. */
    if (nodeFailed(node)) return; /* Already FAILing. */

    failures = clusterNodeFailureReportsCount(node);
    /* Also count myself as a voter if I'm a master. */
    if (nodeIsMaster(myself)) failures++;
    if (failures < needed_quorum) return; /* No weak agreement from masters. */

    serverLog(LL_NOTICE,
        "Marking node %.40s as failing (quorum reached).", node->name);

    /* Mark the node as failing. */
    node->flags &= ~CLUSTER_NODE_PFAIL;
    node->flags |= CLUSTER_NODE_FAIL;
    node->fail_time = mstime();

    /* Broadcast the failing node name to everybody, forcing all the other
     * reachable nodes to flag the node as FAIL. */
    if (nodeIsMaster(myself)) clusterSendFail(node->name);
    clusterDoBeforeSleep(CLUSTER_TODO_UPDATE_STATE|CLUSTER_TODO_SAVE_CONFIG);
}

```

If `master/7001` is the first node that record two `PFAIL` state for node `master/7000`, it will mark it as `FAIL` state and broadcast to all other nodes in the cluster

![fail](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/failover/fail.png)

# ELECT

As soon as a node gets 2 `PFAIL` state for node `master/7000`, it marks it as `FAIL` state

The votes of `PFAIL` state will only valid for thoese node serves at least 1 slot

![elect](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/failover/elect.png)

And broadcast the `FAIL` state info to all reachable nodes

![elect2](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/failover/elect2.png)

Every slave will call `clusterHandleSlaveFailover` in each loop of `clusterCron`

```c
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

```

If our master is in `FAIL` state, our epoch number will be added, and we will ask every other nodes to vote us, but only master serving at least one slot has the right to vote

![elect3](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/failover/elect3.png)

And the master will vote only if meet the following requirements

```c
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

```

Once the slave gets majority of votes, it will become master

![elect4](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/failover/elect4.png)

And because the master now has a higher epoch number, if the old master rejoin the cluster, it will follows the configuration of those with higher epoch number, and becomes the slave of the current master

![elect5](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/failover/elect5.png)

The situations in the real world is more complicated, such as multiply slaves and partition, you need to search for more information such as [raft paper](https://raft.github.io/raft.pdf)