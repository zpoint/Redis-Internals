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

If we want to reshard the slot 9189 from `127.0.0.1:7001` to `127.0.0.1:7000`, We need to execute all the following commands(actually redis sentinal will take care of the commands, we execute them manually for illustration)


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

Before execute

![reshard1](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/reshard1.png)

We can find that `slots_keys_count` only stores nunmber of elements for slots in the current node, not for slots among all the nodes in the cluster

After

    127.0.0.1:7000> CLUSTER SETSLOT 9189 IMPORTING fd1099f4aff0be7fb014e1473c404631a764ffa4
    OK
    127.0.0.1:7001> CLUSTER SETSLOT 9189 MIGRATING 4e1901ce95cfb749b94c435e1f1c123ae0579e79
    OK

![reshard2](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/reshard2.png)

`importing_slots_from[9189]` in `127.0.0.1:7000` now stores `addrB` instead of `NULL` pointer

And `migrating_slots_to[9189]` in `127.0.0.1:7001` now stores `addrA`

In the current state, We can still get `key1` from `127.0.0.1:7001`

    127.0.0.1:7000> get key1
    -> Redirected to slot [9189] located at 127.0.0.1:7001
    "val1"

And after

    127.0.0.1:7001> MIGRATE 127.0.0.1 7000 key1 0 5000
    OK

![reshard3](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/reshard3.png)

The `slots_keys_count[9189]` in `127.0.0.1:7000` becomes 1 and the same field in `127.0.0.1:7001` becomes 0

If we try to get `key1`

    127.0.0.1:7001> get key1
    (error) ASK 9189 127.0.0.1:7000

`127.0.0.1:7001` says you need to `ASK` the `127.0.0.1:7000`, since the `migrating_slots_to[9189]` is set to node in `127.0.0.1:7000` and current node does not have `key1`


    127.0.0.1:7000> ASKING
    OK
    127.0.0.1:7000> get key1
    "val1"

While in `127.0.0.1:7000`, the command follows `ASKING` will not be redirected to other node if the `importing_slots_from[target slot]` is not empty, it will try to execute in the current node and return the result

    127.0.0.1:7000> CLUSTER SETSLOT 9189 NODE 4e1901ce95cfb749b94c435e1f1c123ae0579e79
    OK

![reshard4](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/reshard4.png)

After the `CLUSTER SETSLOT` in `127.0.0.1:7000`, the `addrA`'s bitfield in `9189` in bitmap inside `clusterNodeA` is set and `numslots` is added, the same bitfield in `addrB` is unset and `numslots` minus 1, `importing_slots_from[9189]` is reset to NULL

    127.0.0.1:7001> CLUSTER SETSLOT 9189 NODE 4e1901ce95cfb749b94c435e1f1c123ae0579e79
    OK

![reshard4](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/reshard4.png)

The same happens for node `127.0.0.1:7001`

![reshard5](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/reshard5.png)


# read more
* [Redis Document->cluster-tutorial](https://redis.io/topics/cluster-tutorial)
* [Redis Document->cluster-setslot](https://redis.io/commands/cluster-setslot)