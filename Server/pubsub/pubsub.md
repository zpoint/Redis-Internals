# pubsub

# contents

* [related file](#related-file)
* [sub](#sub)
* [psub](#psub)

# related file

* redis/src/pubsub.c
* redis/src/util.c

# sub

In redis client, If you type the following command

```shell script
127.0.0.1:6379> SUBSCRIBE c100
Reading messages... (press Ctrl-C to quit)
1) "subscribe"
2) "c100"
3) (integer) 1


```

![sub](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/pubsub/sub.png)

[hash table](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/hash.md#OBJ_ENCODING_HT) is used for storing `pubsub_channels`, A [sds](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/sds/sds.md) with string representation `c100` is stored inside the `pubsub_channels` as key, For server, a list of client is stored as value

For client, value field stores a `NULL` pointer

Each client object stores all the channels it subscribed in a dictionary named `client.pubsub_channels`, channel name as key, `NULL` pointer as value

While for `server` object, all channels in all `redis` clients are stored in the `server.pubsub_channels`


If we start another client and subscribe the same channel

```shell script
127.0.0.1:6379> SUBSCRIBE c100
Reading messages... (press Ctrl-C to quit)
1) "subscribe"
2) "c100"
3) (integer) 1


```

![sub2](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/pubsub/sub2.png)

If we unsubscribe for one client, The `channel` in `pubsub_channels` in client instance will be deleted, and the `client` in `server.pubsub_channels`.`value` will also be deleted

The time complexity is the length of `server.pubsub_channels`.`value`, it's a list structure, search in the list is `O(N)`

# psub

If we type the following command in `redis-cli`

```shell script
127.0.0.1:6379> PSUBSCRIBE h*llo
Reading messages... (press Ctrl-C to quit)
1) "psubscribe"
2) "h*llo"
3) (integer) 1

```

![psub](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/pubsub/psub.png)


And subscribe in other client

```shell script
127.0.0.1:6379> PSUBSCRIBE h*llo
Reading messages... (press Ctrl-C to quit)
1) "psubscribe"
2) "h*llo"
3) (integer) 1

```

![psub2](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/pubsub/psub2.png)

`server.pubsub_patterns` is a linked list, each item is a structure with pattern pointer and client pointer inside

`client.pubsub_patterns` is also a linked list, each item is a pattern it subscribed

## publish

The function `stringmatchlen` in `redis/src/util.c` is used for pattern matching

When you enter the `PUBLISH` command

The following function will be called

```c
int pubsubPublishMessage(robj *channel, robj *message) {
    int receivers = 0;
    dictEntry *de;
    listNode *ln;
    listIter li;

    /* Send to clients listening for that channel */
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
    /* Send to clients listening to matching channels */
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

The `list` of clients in `server.pubsub_channels` can be find in O(1) time, and we traverse the `list` and send each client a command

The `list` of patterns in `server.pubsub_patterns` need to be traversed, and for each pattern-client combined structure, the function `stringmatchlen` will be called to check if the pattern matched, if so, send the message to the client
