# pubsub

# 目录

* [相关位置文件](#相关位置文件)
* [sub](#sub)
* [psub](#psub)
* [publish](#publish)

# 相关位置文件

* redis/src/pubsub.c
* redis/src/util.c

# sub

在 redis 客户端中, 如果你输入如下命令

```shell script
127.0.0.1:6379> SUBSCRIBE c100
Reading messages... (press Ctrl-C to quit)
1) "subscribe"
2) "c100"
3) (integer) 1


```

![sub](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/pubsub/sub.png)

`pubsub_channels` 是以 [哈希表](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/hash_cn.md#OBJ_ENCODING_HT) 的结构存储的, 在 `pubsub_channels` 中, key 值是一个 [sds](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/sds/sds_cn.md) 结构, sds 中存储的是 `c100`

对于 server 对象, key 的内容相同, 但是 value 是一个列表, 列表中的每个元素为对应的 `client` 实例

对于 client 对象, value 中存储的是一个空指针

在每一个 client 对象中, 所有这个 client 订阅的 channels 都会存储在名为 `client.pubsub_channels` 的字典中, key 是 channel 的名称, value 是一个空指针

对于 `server`对象来说, 所有不同的 `redis` 客户端的所有的 channels 都存储在 `server.pubsub_channels` 中

如果我们启动另一个客户端, 并且订阅相同的 channel

```shell script
127.0.0.1:6379> SUBSCRIBE c100
Reading messages... (press Ctrl-C to quit)
1) "subscribe"
2) "c100"
3) (integer) 1


```

![sub2](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/pubsub/sub2.png)

如果我们取消一个客户端的订阅, 那么客户端实例中的 `pubsub_channels` 结构中对应的 `channel` 会被删除, 并且 `server.pubsub_channels`.`value` 这个列表也会把对应的 `client` 移除

整个操作的时间复杂度为 `server.pubsub_channels`.`value` 这个列表的长度, 列表搜索的时间复杂度为 `O(N)`

# psub

如果我们在 `redis-cli` 中输入如下命令

```shell script
127.0.0.1:6379> PSUBSCRIBE h*llo
Reading messages... (press Ctrl-C to quit)
1) "psubscribe"
2) "h*llo"
3) (integer) 1

```

![psub](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/pubsub/psub.png)

并且在另一个 client 中进行订阅

```shell script
127.0.0.1:6379> PSUBSCRIBE h*llo
Reading messages... (press Ctrl-C to quit)
1) "psubscribe"
2) "h*llo"
3) (integer) 1

```

![psub2](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/pubsub/psub2.png)

`server.pubsub_patterns` 是一个链表结构, 其中的每一个元素是一个复合结构, 这个结构包含了在指向 pattern 的指针和指向 client 的指针

`client.pubsub_patterns` 也是一个链表, 其中的每一个元素都是该 `client` 实例订阅的 pattern

## publish

目录 `redis/src/util.c` 中的函数 `stringmatchlen` 被用来作为 pattern 匹配的基本函数

当你输入 `PUBLISH` 命令时

下面这个函数会被调用

```c
int pubsubPublishMessage(robj *channel, robj *message) {
    int receivers = 0;
    dictEntry *de;
    listNode *ln;
    listIter li;

    /* 发送给所有监听对应 channel 的客户端 */
    de = dictFind(server.pubsub_channels,channel);
    if (de) {
        list *list = dictGetVal(de);
        listNode *ln;
        listIter li;

        listRewind(list,&li);
        while ((ln = listNext(&li)) != NULL) {
            client *c = ln->value;
            addReplyPubsubMessage(c,channel,message);
            receivers++;
        }
    }
    /* 发送给所有 pattern 匹配上的客户端 */
    if (listLength(server.pubsub_patterns)) {
        listRewind(server.pubsub_patterns,&li);
        channel = getDecodedObject(channel);
        while ((ln = listNext(&li)) != NULL) {
            pubsubPattern *pat = ln->value;

            if (stringmatchlen((char*)pat->pattern->ptr,
                                sdslen(pat->pattern->ptr),
                                (char*)channel->ptr,
                                sdslen(channel->ptr),0))
            {
                addReplyPubsubPatMessage(pat->client,
                    pat->pattern,channel,message);
                receivers++;
            }
        }
        decrRefCount(channel);
    }
    return receivers;
}

```

`server.pubsub_channels` 是一个字典结构, 其中存储 key 值是对应的 channel, value 值是的 client 实例列表, 这个 value 值可以以 O(1) 的时间搜索到, 之后我们遍历这个列表进行推送即可

`server.pubsub_patterns` 中是一个链表, 每个元素存储了对应的复合结构, 其中有 client 信息和对应的 pattern, 需要遍历这个链表, 对每一个 pattern 都调用 `stringmatchlen` 进行匹配看是否能匹配的上, 如果能就给对应的客户端进行推送
