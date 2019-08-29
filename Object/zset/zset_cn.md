# zset

# 目录

* [需要提前了解的知识](#需要提前了解的知识)
* [相关位置文件](#相关位置文件)
* [encoding](#encoding)
	* [OBJ_ENCODING_ZIPLIST](#OBJ_ENCODING_ZIPLIST)
	* [OBJ_ENCODING_SKIPLIST](#OBJ_ENCODING_SKIPLIST)

# 需要提前了解的知识

* [redis 中 hash 对象的 hashtable 实现](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/hash_cn.md#OBJ_ENCODING_HT)
* [redis 中 hash 对象的 ziplist 实现](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/hash_cn.md#OBJ_ENCODING_ZIPLIST)

# 相关位置文件
* redis/src/server.h
* redis/src/t_zset.c

# encoding

## OBJ_ENCODING_ZIPLIST

    127.0.0.1:6379> zadd zset1 5 val3 -1 val1 -3 val2
    (integer) 3

![zset_ziplist](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/zset/zset_ziplist.png)

对于轻量级的 **zset** 会用 [ziplist](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/hash_cn.md#OBJ_ENCODING_ZIPLIST) 的方式来进行存储, **score** 字段紧挨着 **entry**, 并且在这个 **ziplist** 中的值是按 **score** 中存储的分值以升序排列好的方式进行存储的


在配置文件中默认的参数如下

    zset-max-ziplist-entries 128
    zset-max-ziplist-value 64

这两行的意思是如果你的 **zset** 中存储了超过 128 个元素或者其中有任何一个 sds 字符串的长度超过了 64, 那么这个 **ziplist** 会被升级成 **skiplist**

## OBJ_ENCODING_SKIPLIST

这是 **zset** 的内存构造

![zset](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/zset/zset.png)

这是 **skiplist** 的内存构造

![skiplist](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/zset/skiplist.png)

从上图我们可以发现, **zset** 会同时存储一张 [hash 表](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/hash_cn.md#OBJ_ENCODING_HT) 和一个 **skiplist**(跳跃表), 这样可以在不同命令下从不同的结构进行索引

我在我的配置文件下更改了这个值 `zset-max-ziplist-entries 0` 方便演示, 如果你需要了解这个参数的作用, 请参考 [hash 结构的升级](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/hash_cn.md#%E5%8D%87%E7%BA%A7)

    127.0.0.1:6379> del zset1
    (integer) 1
    127.0.0.1:6379> zadd zset1 -1 val1 -3 val2 5 val3 8 val4 9 val5 7 val6 6 val7
    (integer) 7

一个符合 [幂律分布](https://baike.baidu.com/item/%E5%B9%82%E5%BE%8B%E5%88%86%E5%B8%83/4281937?fr=aladdin) 的函数被用来生成跳跃表的层数, `ZSKIPLIST_P` 的值为 `0.25`, 生成 2 层的概率是 `3/4`, 生成 3 层的概率是 `1/4 ^ 2`， 以此类推

![power_law](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/zset/power_law.png)

生成高层数的概率非常低, 生成低层数的概率非常高, 层数越低, 概率越高

`ZSKIPLIST_MAXLEVEL` 中的值为 64, 它限定了层数的上限, 你不能生成一个超过 64 层的节点

    /* 生成一个随机的层数给我们将要创建的跳跃表节点用
     * 这个函数的返回值必须介于 1 到 ZSKIPLIST_MAXLEVEL 之间(两边都是闭区间)
     * 函数的结果需要符合幂律分布, 层数越高, 概率越低 */
    int zslRandomLevel(void) {
        int level = 1;
        while ((random()&0xFFFF) < (ZSKIPLIST_P * 0xFFFF))
            level += 1;
        return (level<ZSKIPLIST_MAXLEVEL) ? level : ZSKIPLIST_MAXLEVEL;
    }

我在源代码中把 `ZSKIPLIST_P` 的值调高到了 `0.7`(只是为了演示的时候更容易生成更高的层级)

![skiplist1](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/zset/skiplist1.png)

从上图可知, 所有插入的值是按照 `score` 中的值以升序的顺序排列的, 并且 `score` 的类型是 `double`([IEEE 754](https://en.wikipedia.org/wiki/IEEE_754-1985)/[IEEE-754标准与浮点数运算](https://blog.csdn.net/m0_37972557/article/details/84594879)), 你能设置的分值只能在双精度浮点数的范围内

通过 [跳跃表](https://zh.wikipedia.org/wiki/%E8%B7%B3%E8%B7%83%E5%88%97%E8%A1%A8) 结构, 你可以从最高层往最底层进行搜索, 层级越高跨度越大, 平均搜索复杂度为 O(log(N))

所以一些通过分值进行查询的命令(比如 `ZREMRANGEBYSCORE`) 可以以 O(log(N)) 的复杂度完成, 而不是线性的复杂度

**zset** 中同时还会存储一份 [hash 表](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/hash_cn.md#OBJ_ENCODING_HT)

![zset_dict](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/zset/zset_dict.png)

在上图的结构中我们发现哈希表中存储的 **score** 是一个双精度浮点数的指针

同理一些通过值进行查询的命令(比如 `ZSCORE`) 可以以 O(1) 的复杂度完成, 而不需要在跳跃表中进行线性搜索



