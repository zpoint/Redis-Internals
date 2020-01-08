# cluster

# contents

* [related file](#related-file)
* [inspect](#inspect)
* [failover](#failover)
* [read more](#read-more)

# related file

* redis/src/cluster.c
* redis/src/cluster.h

# inspect

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



# read more
* [Redis Document->cluster-tutorial](https://redis.io/topics/cluster-tutorial)
