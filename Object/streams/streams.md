# streams

# contents

* [related file](#related-file)
* [memory layout](#memory-layout)
* [encoding](#encoding)
* [internal](#internal)
* [read more](#read-more)


# related file
* redis/src/stream.h
* redis/src/t_stream.c
* redis/src/rax.h
* redis/src/rax.c
* redis/src/listpack.h
* redis/src/listpack.c

# overview

Redis `streams` is introduced in version >= 5.0, it's a MQ like structure, it models a log data structure in a more abstract way, For more overview detail please refer to [Introduction to Redis Streams](https://redis.io/topics/streams-intro)

The block in red in the basic layout of `streams` structure

![streams](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/zset/streams.png)

# encoding

`stremas` is a new and special type, there's currently no general `encoding` for `streams`

    127.0.0.1:6379> object encoding mystream
    "unknown"


# read more
* [Introduction to Redis Streams](https://redis.io/topics/streams-intro)
