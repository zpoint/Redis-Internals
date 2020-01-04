# pubsub

# contents

* [related file](#related-file)
* [pubsub](#pubsub)


# related file

* redis/src/pubsub.c

# pubsub

In redis client, If you type the following command

    127.0.0.1:6379> SUBSCRIBE c100
    Reading messages... (press Ctrl-C to quit)
    1) "subscribe"
    2) "c100"
    3) (integer) 1


![sub](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/pubsub/sub.png)

[hash table](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/hash.md#OBJ_ENCODING_HT) is used for storing `pubsub_channels`, A [sds](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/sds/sds.md) with string repreentatiomn `c100` is stored inside the `pubsub_channels` as key, For server, a list of client is stored as value

For client, value field stores a `NULL` pointer

Each client object stores all the channels it subscribed in a dictionary named `client.pubsub_channels`, channel name as key, `NULL` pointer as value

While in the `server` object, all channels in all `redis` clients are stored in the `server.pubsub_channels`


If we start another client and subscribe the same channel

    127.0.0.1:6379> SUBSCRIBE c100
    Reading messages... (press Ctrl-C to quit)
    1) "subscribe"
    2) "c100"
    3) (integer) 1


![sub2](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/pubsub/sub2.png)

