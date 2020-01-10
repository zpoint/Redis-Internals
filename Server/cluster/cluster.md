# cluster

# contents

* [related file](#related-file)
* [cluster](#cluster)
* [slots](#slots)
* [reshard](#reshard)
* [failover](#failover)
* [read more](#read-more)

# related file

* redis/src/cluster.c
* redis/src/cluster.h

# cluster

If we start a cluster

    mkdir cluster-test
    cd cluster-test
    mkdir 7000 7001 7002

For each directory, do the following configuration

    echo "port 7000
    cluster-enabled yes
    cluster-config-file nodes.conf
    cluster-node-timeout 5000
    appendonly yes" > redis.conf
    ../../src/redis-server ./redis.conf

And in another shell

	redis-cli --cluster create 127.0.0.1:7000 127.0.0.1:7001 127.0.0.1:7002

`clusterCron`  in `redis/src/cluster.c` will be exexuted by `serverCron` if `cluster_enabled` flag is set, and this function will be executed 10 times every second

For client type the following command to connect to any of the nodes

	./redis-cli -h 127.0.0.1 -p 7000 -c

Every server stores the cluster state inside the server status

![clusterState](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/clusterState.png)

# slots

There're totally 16384 slots in the whole cluster, usually it's evenly distributed among all the cluster nodes

> We have 16384 hash slots. The hash slot of a given key is obtained as the least significant 14 bits of the crc16 of the key. However if the key contains the {...} pattern, only the part between { and } is hashed. This may be useful in the future to force certain keys to be in the same node (assuming no resharding is in progress).

This is the hash function of the `key`

    unsigned int keyHashSlot(char *key, int keylen) {
        int s, e; /* start-end indexes of { and } */

        for (s = 0; s < keylen; s++)
            if (key[s] == '{') break;

        /* No '{' ? Hash the whole key. This is the base case. */
        if (s == keylen) return crc16(key,keylen) & 0x3FFF;

        /* '{' found? Check if we have the corresponding '}'. */
        for (e = s+1; e < keylen; e++)
            if (key[e] == '}') break;

        /* No '}' or nothing between {} ? Hash the whole key. */
        if (e == keylen || e == s+1) return crc16(key,keylen) & 0x3FFF;

        /* If we are here there is both a { and a } on its right. Hash
         * what is in the middle between { and }. */
        return crc16(key+s+1,e-s-1) & 0x3FFF;
    }

If we execute the `set` command

    127.0.0.1:7000> SET key1 val1
    -> Redirected to slot [9189] located at 127.0.0.1:7001
    OK

can be represented as

![slots](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/slots.png)

or

![slots2](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/slots2.png)

# reshard

![reshard0](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/reshard0.png)

The above diagram is part of the cluster status in `127.0.0.1:7001`

`myself` points to the master cluster node for current server instance

`currentEpoch` is the epoch number in current node

`size` is the cluster size

`nodes` is a dictionary, stores all the nodes as `clusterNode` structure in the cluster, `node ID` as key, and `clusterNode` as value, `clusterNode` stores all the information needed, such as slot bitmap

`migrating_slots_to` is an array of pointer, length 16384, if there's any slot in  `migrating_slots_to` state, the `migrating_slots_to[i]` will stores the pointer pointed to that `clusterNode`

`importing_slots_from` is similar to `migrating_slots_to`

`slots` is an array of pointer, length is 16384, each pointer points to the `clusterNode` structure the slot belongs to

`slots_keys_count` stores number of emelents for every slots

And

`key1` is currently in slot 9189 and slot 9189 in located in `127.0.0.1:7001`

If we want to reshard the slot 9189 from `127.0.0.1:7001` to `127.0.0.1:7000`

    127.0.0.1:7000> CLUSTER SETSLOT 9189 IMPORTING fd1099f4aff0be7fb014e1473c404631a764ffa4
    OK
    127.0.0.1:7001> CLUSTER SETSLOT 9189 MIGRATING 4e1901ce95cfb749b94c435e1f1c123ae0579e79
    OK
    127.0.0.1:7001> CLUSTER GETKEYSINSLOT 9189 100
    1) "key1"
    127.0.0.1:7001> MIGRATE 127.0.0.1 7000 key1 0 5000
    OK
    127.0.0.1:7000> CLUSTER SETSLOT 9189 NODE 4e1901ce95cfb749b94c435e1f1c123ae0579e79
    OK
    127.0.0.1:7001> CLUSTER SETSLOT 9189 NODE 4e1901ce95cfb749b94c435e1f1c123ae0579e79
    OK
    127.0.0.1:7002> CLUSTER SETSLOT 9189 NODE 4e1901ce95cfb749b94c435e1f1c123ae0579e79
    OK


# read more
* [Redis Document->cluster-tutorial](https://redis.io/topics/cluster-tutorial)
* [Redis Document->cluster-setslot](https://redis.io/commands/cluster-setslot)