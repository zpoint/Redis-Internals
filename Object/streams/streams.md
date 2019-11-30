# streams

# contents

* [prerequisites](#prerequisites)
* [related file](#related-file)
* [memory layout](#memory-layout)
* [encoding](#encoding)
* [internal](#internal)
* [read more](#read-more)

# prerequisites

[rax](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/rax/rax.md)(a redis implementation [radix tree](https://redis.io/topics/streams-intro))


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

![streams](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/streams/streams.png)

# encoding

`stremas` is a new and special type, the `object encoding` returns `unknown` but it actually has a type label with `OBJ_STREAM`

    127.0.0.1:6379> xadd mystream * key1 val1
    "1575041330626-0"
    127.0.0.1:6379> object encoding mystream
    "unknown"

# internal





[rax](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/rax/rax.md) is a redis implementation [radix tree](https://en.wikipedia.org/wiki/Radix_tree), it a space-optimized prefix tree used for storing consumer groups name and some other information



# read more
* [Introduction to Redis Streams](https://redis.io/topics/streams-intro)
