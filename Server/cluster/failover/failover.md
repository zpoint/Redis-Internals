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

    127.0.0.1:7003> CLUSTER MEET 127.0.0.1 7000
    OK
    127.0.0.1:7003> CLUSTER NODES
    75ddecf9169096eeff03ec60f868859333c824a1 127.0.0.1:7003@17003 myself,master - 0 1579513065000 0 connected
    ba060347b647e2cb050eea666f087fd08d4e42d7 127.0.0.1:7001@17001 master - 0 1579513065256 2 connected 5461-10922
    c62cfa7c26ec449462cceb5fd45cac121d866d44 127.0.0.1:7002@17002 master - 0 1579513066283 3 connected 10923-16383
    89c21152eedde6d9dec2fcebf4330b39810bc179 127.0.0.1:7000@17000 master - 0 1579513066797 1 connected 0-5460
    127.0.0.1:7003> CLUSTER REPLICATE 89c21152eedde6d9dec2fcebf4330b39810bc179

And set it as a slave of node `127.0.0.1:7000`

    127.0.0.1:7003> CLUSTER REPLICATE 89c21152eedde6d9dec2fcebf4330b39810bc179
    OK
    127.0.0.1:7003> CLUSTER NODES
    75ddecf9169096eeff03ec60f868859333c824a1 127.0.0.1:7003@17003 myself,slave 89c21152eedde6d9dec2fcebf4330b39810bc179 0 1579513200000 0 connected
    ba060347b647e2cb050eea666f087fd08d4e42d7 127.0.0.1:7001@17001 master - 0 1579513201517 2 connected 5461-10922
    c62cfa7c26ec449462cceb5fd45cac121d866d44 127.0.0.1:7002@17002 master - 0 1579513201000 3 connected 10923-16383
    89c21152eedde6d9dec2fcebf4330b39810bc179 127.0.0.1:7000@17000 master - 0 1579513201833 1 connected 0-5460


If we fail the node `127.0.0.1:7000`

    ^C943:signal-handler (1579016631) Received SIGINT scheduling shutdown...
    943:M 14 Jan 2020 23:43:51.330 # User requested shutdown...
    943:M 14 Jan 2020 23:43:51.330 * Calling fsync() on the AOF file.
    943:M 14 Jan 2020 23:43:51.330 # Redis is now ready to exit, bye bye...
    zpoint@bogon 7000 %

We can find that the node `127.0.0.1:7003` becomes master and the epoch num is the greatest

    127.0.0.1:7003> CLUSTER NODES
    ba060347b647e2cb050eea666f087fd08d4e42d7 127.0.0.1:7001@17001 master - 0 1579572427408 2 connected 5461-10922
    89c21152eedde6d9dec2fcebf4330b39810bc179 127.0.0.1:7000@17000 master,fail - 1579572351865 1579572351452 5 disconnected
    75ddecf9169096eeff03ec60f868859333c824a1 127.0.0.1:7003@17003 myself,master - 0 1579572426000 7 connected 0-5460
    c62cfa7c26ec449462cceb5fd45cac121d866d44 127.0.0.1:7002@17002 master - 0 1579572426898 3 connected 10923-16383


`clusterHandleSlaveFailover`

# PFAIL

What happended when we fail the node `127.0.0.1:7000` ?

![overview](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/failover/overview.png)

For `master/7001`, `master/7002` and `slave/7003`

`clusterCron` will mark the node as `PFAIL` state if the current node can't receive the `PONG` message for the target node in `cluster-node-timeout` millseconds, default value is 15s

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

![pfail](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/failover/pfail.png)

`master/7001`, `master/7002` and `slave/7003` will communicate with `master/7000` via [gossip](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/gossip/gossip.md) protocol, After `cluster-node-timeout` millseconds they are not able to receive the `PONG` message, each of them will mark `master/7000` as `PFAIL` state

![pfail2](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/failover/pfail2.png)

Every node will has it's own `cluster-node-timeout` configuration, and the time they send `PING` message will various, but if the node `master/7000` really fails, after `timestamp in PING` + `cluster-node-timeout`, majority of node will mark `master/7000` as `PFAIL`

# FAIL

The `PFAIL` state of the `master/7000` will propagate via the [gossip](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/gossip/gossip.md) protocol, whenever a node receive a node with `PFAIL` state, `markNodeAsFailingIfNeeded` will be called

`markNodeAsFailingIfNeeded` will mark the node `master/7000` as `FAIL` state if there're at least `(server.cluster->size / 2) + 1` says the node is in `PFAIL` state, which is 3 in the current cluster

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

If `master/7001` is the first node that record three `PFAIL` state for node `master/7000`, it will mark it as `FAIL` state and broadcast to all other nodes in the cluster

![fail](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/failover/fail.png)

# ELECT

