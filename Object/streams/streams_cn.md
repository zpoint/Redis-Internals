# streams

# 目录

* [需要提前了解的知识](#需要提前了解的知识)
* [相关位置文件](#相关位置文件)
* [概览](#概览)
* [内部实现](#内部实现)
    * [xadd](#xadd)
    * [xdel](#xdel)
    * [xrange](#xrange)
    * [xread](#xread)
    * [consumer groups](#consumer-groups)
* [更多资料](#更多资料)

# 需要提前了解的知识

* [rax](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/rax/rax_cn.md)(redis 实现的前缀树)
* [redis listpack 实现](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/listpack/listpack_cn.md)


# 相关位置文件
* redis/src/stream.h
* redis/src/t_stream.c
* redis/src/rax.h
* redis/src/rax.c
* redis/src/listpack.h
* redis/src/listpack.c

# 概览

Redis 的 `streams` 结构在版本号 5.0 以后引入, 它抽象的模拟了日志系统的结构, 如果你需要了解更多的相关介绍和使用方法, 请参考 [Introduction to Redis Streams](https://redis.io/topics/streams-intro)

红色标记的是 `streams` 结构的基本构造

![streams](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/streams/streams.png)

# 内部实现

`stremas` 是一个新引入的特殊的类型, `object encoding` 返回的是 `unknown` 但它实际上有个类型名为 `OBJ_STREAM`

```shell script
127.0.0.1:6379> xadd mystream * key1 128
"1576480551233-0"
127.0.0.1:6379> object encoding mystream
"unknown"

```

和前面的 [rax](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/rax/rax_cn.md), [listpack](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/listpack/listpack_cn.md) 等结构组合在一起, `mystream` 总共由 3 部分构成

第一部分是 `robj`, 每个 redis 对象实例都会有一个最基本的结构来存储它实际的类型, 编码和对应的结构的位置

第二部分是一个 `rax`, 用作存储 `stream ID`

第三部分是 `listpack`, `rax` 下的每一个 `key` 节点都会把对应的 `keys` 和 `values` 的值存在这个 `listpack` 结构中

![mystream](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/streams/mystream.png)

### xadd

如果我们再插入一个键对值到同个 redis key 中

```shell script
127.0.0.1:6379> xadd mystream * key1 val1
"1576486352510-0"

```

![mystream2](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/streams/mystream2.png)

我们可以发现, `stream` 结构用来存储了所有插入的数据, `rax` 指向了一个基数树, 它的第一个入口是第一个插入时的 `ID`, 并且把对应的值插入到基数树 key 节点锁关联的对应的 `listpack` 结构中

`length` 表示当前的 `stream` 中存储了多少个值(实际上就是基数树中所有的 `listpack` 中所有的未删除元素的和)

`last_id` 存储了  `stream` 最顶部的 `ID` 值, 可以是用户指定的也可以是自动生成的

`cgroups` 把 `consumer groups` 存储在了一个 `rax` 中

```c
int streamAppendItem(stream *s, robj **argv, int64_t numfields, streamID *added_id, streamID *use_id) {
    raxIterator ri;
    raxStart(&ri,s->rax);
    raxSeek(&ri,"$",NULL,0);
    /* ... */
    /*获取到一个指向尾部节点的相关联的 listpack 指针 */
    if (raxNext(&ri)) {
        lp = ri.data;
        lp_bytes = lpBytes(lp);
    }
    /* ... */
    /* listpack初始化的由以下部分构成
     *
     * +-------+---------+------------+---------+--/--+---------+---------+-+
     * | count | deleted | num-fields | field_1 | field_2 | ... | field_N |0|
     * +-------+---------+------------+---------+--/--+---------+---------+-+
     * /
    if (lp != NULL) {
        if (server.stream_node_max_bytes &&
            lp_bytes > server.stream_node_max_bytes)
        {
            lp = NULL;
        } else if (server.stream_node_max_entries) {
            int64_t count = lpGetInteger(lpFirst(lp));
            if (count > server.stream_node_max_entries) lp = NULL;
        }
    }
    /* 真正进行插入的时候
     *
     * +-----+--------+----------+-------+-------+-/-+-------+-------+--------+
     * |flags|entry-id|num-fields|field-1|value-1|...|field-N|value-N|lp-count|
     * +-----+--------+----------+-------+-------+-/-+-------+-------+--------+
     *
     * 如果 SAMEFIELD flag 置为 1, 我们只用插入 value 不用插入 key
     *
     * +-----+--------+-------+-/-+-------+--------+
     * |flags|entry-id|value-1|...|value-N|lp-count|
     * +-----+--------+-------+-/-+-------+--------+
     * ...
     * /
}

```

从以上的代码可知, 当你往 `stream` 插入一个新的对象时, 他会找对最新的一个 `listpack` 结构, 并且持续的往这个 `listpack` 尾部插入一直到这个 `listpack` 满了为止(大小超过 `stream-node-max-bytes`(默认 4kb) 或者长度超过 `stream-node-max-entries`(默认100))

### xdel

如果我们删除最后一个 `ID`

```shell script
127.0.0.1:6379> xdel mystream 1576486352510-0
(integer) 1

```

![xdel](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/streams/xdel.png)

唯一的区别是就是 `flag` 中的第一个位被置为了 1, 并且 `listpack` 的前两个元素, num items 减小了 1, num deleted 增加了 1

下面是截取的部分源代码

```c
void streamIteratorRemoveEntry(streamIterator *si, streamID *current) {
    unsigned char *lp = si->lp;
    int64_t aux;

    /* 我们不去真正的对这个 entry 进行删除, 我们只是把它标记成已删除,
     * 并且在 listpack 头中增加已删除的数目
     * /
    int flags = lpGetInteger(si->lp_flags);
    flags |= STREAM_ITEM_FLAG_DELETED;
    lp = lpReplaceInteger(lp,&si->lp_flags,flags);

    /* 更改头部的未删除和已删除数目 */
    unsigned char *p = lpFirst(lp);
    aux = lpGetInteger(p);

    if (aux == 1) {
        /* 如果这是 listpack 的最后一个元素, 直接删掉这个节点和对应的 listpack */
        lpFree(lp);
        raxRemove(si->stream->rax,si->ri.key,si->ri.key_len,NULL);
    } else {
        /* 如果是普通的情况, 我们直接更改更改头部的未删除和已删除数目 */
        lp = lpReplaceInteger(lp,&p,aux-1);
        p = lpNext(lp,p); /* 指向已删除 */
        aux = lpGetInteger(p);
        lp = lpReplaceInteger(lp,&p,aux+1);

        /* 更新 listpack 指针 */
        if (si->lp != lp)
            raxInsert(si->stream->rax,si->ri.key,si->ri.key_len,lp,NULL);
    }

    /* 更新 stream 结构的计数 */
    si->stream->length--;
    /* ... */
}


```

如果我们尝试插入同样的 `ID`, `key` 和 `value`

```shell script
127.0.0.1:6379> xadd mystream 1576486352510-0 key1 val1
(error) ERR The ID specified in XADD is equal or smaller than the target stream top item

```

我们可以发现, 即使某个对应的 `ID` 已经被删除了, 你也无法再次插入这个相同的 `ID`

```shell script
127.0.0.1:6379> xadd mystream 1576486352510-1 key1 val1
"1576486352510-1"

```

你必须插入一个比顶部的 `ID` 值还要大的 `ID`(即使这个 `ID` 已经被删除了), 因为这个对比顶部的过程是拿你插入的 `ID` 和缓存的 `last_id` 这个字段做比较的, 并不是直接去搜索最后一个元素, `last_id` 没有记录是否删除这样的信息

```c
if (use_id && streamCompareID(use_id,&s->last_id) <= 0) return C_ERR;

```

### xrange

我们已经了解了 `streams` 数据结构,  `xrange` 使用的算法就很直观了

![xrange](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/streams/xrange.png)

如果我们发送一个 `xrange mystream ID3 ID4` 这样的命令到 redis 服务上, 并且起始和结束位置和上图所示相同

根据对应的基数树往下搜索, 找到 `master ID` 比 `ID3` 小的第一个 `listpack`, 遍历这个 `listpack`, 对于 `listpack` 中的每个元素, 如果当前的这个 ID(`master ID` + `offset`) 比 `ID4` 小并且比 `ID3` 大, 把这个元素加到返回列表中

如果这是当前的 `listpack` 的最后一个元素, 往下找到下一个`radix tree` 中的 key 节点和对应的 `listpack` 并重复上一步

![xrange2](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/streams/xrange2.png)

### xread

```shell script
127.0.0.1:6379> XREAD COUNT 5 STREAMS mystream 0
1) 1) "mystream"
   2) 1) 1) "1576480551233-0"
         2) 1) "key1"
            2) "128"
      2) 1) "1576486352510-1"
         2) 1) "key1"
            2) "val1"
127.0.0.1:6379> XREAD COUNT 5 STREAMS mystream 1576486352510-2
(nil)
127.0.0.1:6379> XREAD COUNT 5 BLOCK 5000 STREAMS mystream 1576486352510-2
(nil)
(5.04s)

```

`xread` 会判断你提供的 `ID` 是否小于 `streams->last_id`, 如果是的话, 可以马上返回的客户端结果, 如果不是的话, 检查 `block` 参数看是否可以马上返回

这里用到的查找算法和 `xrange` 使用的相同

### consumer groups

如果我们创建一个 `consumer group`

```shell script
127.0.0.1:6379> XGROUP CREATE mystream my_group 0
OK

```

并且给这个 `consumer group` 增加一个用户, 并消费一个元素

```shell script
127.0.0.1:6379> XREADGROUP GROUP my_group user1 COUNT 1 STREAMS mystream >
1) 1) "mystream"
   2) 1) 1) "1576480551233-0"
         2) 1) "key1"
            2) "128"

```

![xgroup](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/streams/xgroup.png)

`streamCG` 中的 `pel` 字段和 `consumers` 字段存储了什么内容 ?

![xgroup_inner](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/streams/xgroup_inner.png)

如果我们对同个用户再消费一个元素

```shell script
127.0.0.1:6379> XREADGROUP GROUP my_group user1 COUNT 1 STREAMS mystream >
1) 1) "mystream"
   2) 1) 1) "1576486352510-1"
         2) 1) "key1"
            2) "val1"

```

![xgroup_inner2](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/streams/xgroup_inner2.png)

现在我们知道为什么基数树中的 `ID` 以大端的方法进行存储了, 因为 `ID` 在大部分情况下是基于时间序列生成的, 相邻的元素间隔的时间越小, 整体上存储所耗费的空间也就越小, 并且不会影响访问对应对象的速度

比如在这个例子中, 这两个 `ID` 前5个字节是相同的, 所以第一个节点就被压缩了, 这个节点共用了这5个字节, 再往下的两个不同的节点存储了两个剩下的不同的部分, 拼起来就是原本的 `ID`

这个基数树把 `ID` 当成 key, 并且每个节点下面存储了对应的 `streamNACK` 结构

`streamNACK` 表示了

> consumer group 里当前等待中未确认的消息

定义如下

```c
typedef struct streamNACK {
    mstime_t delivery_time;     /* 这个消息最后一次投递的时间 */
    uint64_t delivery_count;    /* 这个消息投递的次数 */
    streamConsumer *consumer;   /* 最后一次投递给的消费者的指针 */
} streamNACK;

```

如果我们确认第一条消息, 标记这个`ID` 为已处理

```shell script
127.0.0.1:6379> XACK mystream my_group 1576480551233-0
(integer) 1

```

![xgroup_after_xack](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/streams/xgroup_after_xack.png)

这个标记的 `ID` 会从 `streamCG->pel` 和 `streamConsumer->pel` 中删除

我们可以发现, `streamCG->pel` 存储了一个消费组中的所有消费者所消费的未确认 `ID`, 而 `streamCG->consumers` 存储了所有的消费者名称作为 key, `streamConsumer` 对象作为值, 并且 `streamConsumer` 也在一个基数树中存储了一份自己所消费的未确认 `ID`

# 更多资料
* [Introduction to Redis Streams](https://redis.io/topics/streams-intro)
