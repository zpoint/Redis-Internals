# gossip

# contents

* [cluster bus](#cluster-bus)
* [when will the MSG be sent](#when-will-the-MSG-be-sent)
* [ping](#ping)
* [pong](#pong)

# cluster bus

From [cluster-tutorial](https://redis.io/topics/cluster-tutorial)

> Every Redis Cluster node requires two TCP connections open. The normal Redis TCP port used to serve clients, for example 6379, plus the port obtained by adding 10000 to the data port, so 16379 in the example.

> This second high port is used for the Cluster bus, that is a node-to-node communication channel using a binary protocol. The Cluster bus is used by nodes for failure detection, configuration update, failover authorization and so forth. Clients should never try to communicate with the cluster bus port, but always with the normal Redis command port, however make sure you open both ports in your firewall, otherwise Redis cluster nodes will be not able to communicate.

> The command port and cluster bus port offset is fixed and is always 10000.

> Note that for a Redis Cluster to work properly you need, for each node:

> The normal client communication port (usually 6379) used to communicate with clients to be open to all the clients that need to reach the cluster, plus all the other cluster nodes (that use the client port for keys migrations).
> The cluster bus port (the client port + 10000) must be reachable from all the other cluster nodes.
> If you don't open both TCP ports, your cluster will not work as expected.

> The cluster bus uses a different, binary protocol, for node to node data exchange, which is more suited to exchange information between nodes using little bandwidth and processing time.

# when will the MSG be sent

Redis currently support the following MSG types

    #define CLUSTERMSG_TYPE_PING 0          /* Ping */
    #define CLUSTERMSG_TYPE_PONG 1          /* Pong (reply to Ping) */
    #define CLUSTERMSG_TYPE_MEET 2          /* Meet "let's join" message */
    #define CLUSTERMSG_TYPE_FAIL 3          /* Mark node xxx as failing */
    #define CLUSTERMSG_TYPE_PUBLISH 4       /* Pub/Sub Publish propagation */
    #define CLUSTERMSG_TYPE_FAILOVER_AUTH_REQUEST 5 /* May I failover? */
    #define CLUSTERMSG_TYPE_FAILOVER_AUTH_ACK 6     /* Yes, you have my vote */
    #define CLUSTERMSG_TYPE_UPDATE 7        /* Another node slots configuration */
    #define CLUSTERMSG_TYPE_MFSTART 8       /* Pause clients for manual failover */
    #define CLUSTERMSG_TYPE_MODULE 9        /* Module cluster API message. */
    #define CLUSTERMSG_TYPE_COUNT 10        /* Total number of message types. */


`clusterCron` will traverse every node and call `clusterSendPing` if there're currently not active connection for current node and the target node

`clusterCron` will also ping 1-5 random nodes every second

    /* Ping some random node 1 time every 10 iterations, so that we usually ping
     * one random node every second. */
    if (!(iteration % 10)) {
        int j;

        /* Check a few random nodes and ping the one with the oldest
         * pong_received time. */
        for (j = 0; j < 5; j++) {
            de = dictGetRandomKey(server.cluster->nodes);
            clusterNode *this = dictGetVal(de);

            /* Don't ping nodes disconnected or with a ping currently active. */
            if (this->link == NULL || this->ping_sent != 0) continue;
            if (this->flags & (CLUSTER_NODE_MYSELF|CLUSTER_NODE_HANDSHAKE))
                continue;
            if (min_pong_node == NULL || min_pong > this->pong_received) {
                min_pong_node = this;
                min_pong = this->pong_received;
            }
        }
        if (min_pong_node) {
            serverLog(LL_DEBUG,"Pinging node %.40s", min_pong_node->name);
            clusterSendPing(min_pong_node->link, CLUSTERMSG_TYPE_PING);
        }
    }

`clusterCron` will traverse every node again and send ping in the following situations

    /* If we have currently no active ping in this instance, and the
     * received PONG is older than half the cluster timeout, send
     * a new ping now, to ensure all the nodes are pinged without
     * a too big delay. */
    if (node->link &&
        node->ping_sent == 0 &&
        (now - node->pong_received) > server.cluster_node_timeout/2)
    {
        clusterSendPing(node->link, CLUSTERMSG_TYPE_PING);
        continue;
    }

    /* If we are a master and one of the slaves requested a manual
     * failover, ping it continuously. */
    if (server.cluster->mf_end &&
        nodeIsMaster(myself) &&
        server.cluster->mf_slave == node &&
        node->link)
    {
        clusterSendPing(node->link, CLUSTERMSG_TYPE_PING);
        continue;
    }

So, If we're nodeC, We will have a connection will nodeA, nodeB and all other slaves of nodeA, and follow the above rules to `PING` some other nodes periodically

![overview](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/gossip/overview.png)

# ping

This is what the `MSG` between nodes looks like

![msg](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/gossip/msg.png)


And the `PING` message

![ping](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/gossip/ping.png)

`clusterMsg` stores the information about the sender, including it's `epoch` number, slot bit map, port number, cluster bus port number and etc

In the `clusterMsgData` fields, Every gossip message will carry some nodes information in the cluster, so after several gossip `PING/PONG` exchange, a node will know every other nodes' information finally

The exact number of some is` min(freshnodes, wanted)`

    /* freshnodes is the max number of nodes we can hope to append at all:
     * nodes available minus two (ourself and the node we are sending the
     * message to). However practically there may be less valid nodes since
     * nodes in handshake state, disconnected, are not considered. */
    int freshnodes = dictSize(server.cluster->nodes)-2;

    /*
     * How many gossip sections we want to add? 1/10 of the number of nodes
     * and anyway at least 3. Why 1/10?
     * The reason is in redis/src/cluster.c
     */
    wanted = floor(dictSize(server.cluster->nodes)/10);

So, the node in `clusterMsgData` fields chosen is random

# pong

Every `PING` message will reply with a `PONG` message

The reply of `PING` message is `PONG` message, The `PONG` message is nearly the same as `PING` message except for the `hdr->type` field


	int clusterProcessPacket(clusterLink *link) {
    	/* ... */
        /* Initial processing of PING and MEET requests replying with a PONG. */
        if (type == CLUSTERMSG_TYPE_PING || type == CLUSTERMSG_TYPE_MEET) {
            /* ... */
            clusterSendPing(link,CLUSTERMSG_TYPE_PONG);
        }
        /* ... */
    }

    /* Build the message header. hdr must point to a buffer at least
     * sizeof(clusterMsg) in bytes. */
    void clusterBuildMessageHdr(clusterMsg *hdr, int type) {
        /* ... */
        hdr->ver = htons(CLUSTER_PROTO_VER);
        hdr->sig[0] = 'R';
        hdr->sig[1] = 'C';
        hdr->sig[2] = 'm';
        hdr->sig[3] = 'b';
        hdr->type = htons(type);
        memcpy(hdr->sender,myself->name,CLUSTER_NAMELEN);
        /* ... */
    }

If you're interested in other type of message, please read the source code in `redis/src/cluster.c`
