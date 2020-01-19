# cluster

# contents

* [相关位置文件](#相关位置文件)
* [集群](#集群)
* [槽](#槽)
* [reshard](#reshard)
* [gossip](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/gossip/gossip.md)
* [failover](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/failover/failover.md)
* [更多资料](#更多资料)

# 相关位置文件

* redis/src/cluster.c
* redis/src/cluster.h

# 集群


如果我们启动一个集群

    mkdir cluster-test
    cd cluster-test
    mkdir 7000 7001 7002

在每一个目录下做如下的配置

    echo "port 7000
    cluster-enabled yes
    cluster-config-file nodes.conf
    cluster-node-timeout 5000
    appendonly yes" > redis.conf
    ../../src/redis-server ./redis.conf

再开启一个终端, 输入如下命令

	redis-cli --cluster create 127.0.0.1:7000 127.0.0.1:7001 127.0.0.1:7002

当前的 redis 实例的 `cluster_enabled` 标记位为开启状态时, `redis/src/cluster.c` 中的 `clusterCron` 会每 1 秒执行 10 次

对于 redis 客户端来说, 输入如下命令和任意一个 redis 节点进行连接

	./redis-cli -h 127.0.0.1 -p 7000 -c

每一个节点在自身的服务状态里面都都存储了当前集群的状态

![clusterState](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/clusterState.png)

# 槽

一个集群中总共有 16384 个槽, 通常, 这些槽均匀的分布在各个集群的节点之中

> 我们一共拥有 16384 个哈希槽, 哈希槽的 key 是通过对用户的 key 进行 crc16, 并取最右边的 14 个 bit 生成的. 然而, 如果用户的 key 包含 {...} 这个样子的字符串的话, 只有 { 中间的部分 } 会被进行哈希. 这个策略可以满足用户希望强制不同的 key 映射到相同节点的需求(假设没有正在进行 resharding)

这是用户 `key` 对应的哈希函数

    unsigned int keyHashSlot(char *key, int keylen) {
        int s, e; /* { 中间部分 } 的开始下标和结束下标 */

        for (s = 0; s < keylen; s++)
            if (key[s] == '{') break;

        /* 不存在 '{' ? 对整个用户 key 进行哈希. 这是最基本的情况 */
        if (s == keylen) return crc16(key,keylen) & 0x3FFF;

        /* 存在 '{', 搜索下是否包含 '}' */
        for (e = s+1; e < keylen; e++)
            if (key[e] == '}') break;

        /* 不包含 '}' 或者 {} 中间为空, 对整个用户 key 进行哈希 */
        if (e == keylen || e == s+1) return crc16(key,keylen) & 0x3FFF;

        /* 到了这里, 表示 { 中间 } 有其他字符, 对中间部分进行哈希 */
        return crc16(key+s+1,e-s-1) & 0x3FFF;
    }

如果我们执行 `set` 命令

    127.0.0.1:7000> SET key1 val1
    -> Redirected to slot [9189] located at 127.0.0.1:7001
    OK

可以进行如下表示

![slots](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/slots.png)

或者表示成下面这样

![slots2](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/slots2.png)

# reshard

![reshard0](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/reshard0.png)

上面的图表是 `127.0.0.1:7001` 里面存储的集群状态的一部分表示

`myself` 指向了集群中当前实例这个节点

`currentEpoch` 是当前节点的 epoch 数

`size` 表示这个集群的大小(节点数量)

`nodes` 是一个字典结构, 以 `clusterNode` 结构存储了这个集群的所有节点, key 是 `node ID`, value 是 `clusterNode` 结构, `clusterNode` 存储了所有该节点必要的信息, 比如槽的 bitmap

`migrating_slots_to` 是一个指针数组, 长度为 16384, 如果当前节点中有任何一个槽是处于 `migrating_slots_to` 状态的话, `migrating_slots_to[i]` 将会存储一个指向对应的 `clusterNode` 的指针

`importing_slots_from` 和 `migrating_slots_to` 的结构类似

`slots` 是一个指针数组, 长度为 16384, 每个指针指向了当前这个槽所属的 `clusterNode` 对象

`slots_keys_count` 存储了每个槽对应的元素数目

并且

`key1` 当前存储在槽 9189, 而 9189 位于 `127.0.0.1:7001` 中

如果我们想要把槽 9189 从 `127.0.0.1:7001` reshard 到 `127.0.0.1:7000` 中, 我们需要按顺序执行下面的命令(实际上 redis sentinal 会简化这些步骤, 我们手动执行只是为了解释说明)


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

在执行任何一个命令之前

![reshard1](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/reshard1.png)

我们可以发现 `slots_keys_count` 存储的是当前节点的槽的元素数目, 而不是整个集群中的槽的元素数目

在执行下面两个命令之后

    127.0.0.1:7000> CLUSTER SETSLOT 9189 IMPORTING fd1099f4aff0be7fb014e1473c404631a764ffa4
    OK
    127.0.0.1:7001> CLUSTER SETSLOT 9189 MIGRATING 4e1901ce95cfb749b94c435e1f1c123ae0579e79
    OK

![reshard2](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/reshard2.png)

`127.0.0.1:7000` 中的 `importing_slots_from[9189]` 现在存储了 `addrB` 而不是 `NULL` 指针

并且 `127.0.0.1:7001` 中的 `migrating_slots_to[9189]` 现在存储了 `addrA`

在当前的状态下, 我们仍然可以从 `127.0.0.1:7001` 获得 `key1` 对应的值

    127.0.0.1:7000> get key1
    -> Redirected to slot [9189] located at 127.0.0.1:7001
    "val1"

在执行下面的命令之后

    127.0.0.1:7001> MIGRATE 127.0.0.1 7000 key1 0 5000
    OK

![reshard3](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/reshard3.png)

`127.0.0.1:7000` 中的 `slots_keys_count[9189]` 变成了 1 并且 `127.0.0.1:7001` 中的同个位置变成了 0

如果我们此时再尝试获取 `key1`

    127.0.0.1:7001> get key1
    (error) ASK 9189 127.0.0.1:7000

`127.0.0.1:7001` 说你需要去 `127.0.0.1:7000` 执行 `ASK` 命令, 因为 `migrating_slots_to[9189]` 已经被设置为节点 `127.0.0.1:7000` 并且当前的节点没有 `key1` 这个值

    127.0.0.1:7000> ASKING
    OK
    127.0.0.1:7000> get key1
    "val1"

在 `127.0.0.1:7000` 中, 如果 `importing_slots_from[target slot]` 不为空, 那么跟着 `ASKING` 之后的命令命中到对应的槽中的话, 这个命令不会被重定向到另外的节点, 它会尝试在当前的节点执行并返回结果

    127.0.0.1:7000> CLUSTER SETSLOT 9189 NODE 4e1901ce95cfb749b94c435e1f1c123ae0579e79
    OK

![reshard4](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/reshard4.png)

在 `127.0.0.1:7000` 执行 `CLUSTER SETSLOT` 之后, `clusterNodeA` 中的  `addrA` 的 bitmap 中的 `9189` 槽对应的 bit 被置为 1, 并且 `numslots` 也加了 1, `addrB` bitmap 中同个位置的 bit 被置为 0, 并且 `numslots`减了 1, `importing_slots_from[9189]` 也被重置为 `NULL` 指针

    127.0.0.1:7001> CLUSTER SETSLOT 9189 NODE 4e1901ce95cfb749b94c435e1f1c123ae0579e79
    OK

同样的情况在节点 `127.0.0.1:7001` 也会发生一遍

![reshard5](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/reshard5.png)



# 更多资料
* [Redis Document->cluster-tutorial](https://redis.io/topics/cluster-tutorial)
* [Redis Document->cluster-setslot](https://redis.io/commands/cluster-setslot)
* [Redis Document->cluster-spec](https://redis.io/topics/cluster-spec)