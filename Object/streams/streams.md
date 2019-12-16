# streams

# contents

* [prerequisites](#prerequisites)
* [related file](#related-file)
* [memory layout](#memory-layout)
* [internal](#internal)
* [read more](#read-more)

# prerequisites

* [rax](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/rax/rax.md)(a redis implementation [radix tree](https://redis.io/topics/streams-intro))
* [redis listpack implementation](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/listpack/listpack.md)


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

# internal

`stremas` is a new and special type, the `object encoding` returns `unknown` but it actually has a type named `OBJ_STREAM`

    127.0.0.1:6379> xadd mystream * key1 128
    "1576480551233-0"
    127.0.0.1:6379> object encoding mystream
    "unknown"

Together with [rax](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/rax/rax.md) and [listpack](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/listpack/listpack.md) in [prerequisites](#prerequisites), there're totally 3 parts in `mystream`

The first part is `robj`, every redis object will have this basic part to indicate that the actual type, encoding and location of the target object stored in `ptr` field

The second part is `rax`, it's used for storing the `stream ID`

The third part is `listpack`, every key node in the `rax` stores the according `keys` and `values` in the `listpack` structure

![mystream](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/streams/mystream.png)

## stream

If we add a new value to the same key again

    127.0.0.1:6379> xadd mystream * key1 val1
    "1576486352510-0"

![mystream2](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/streams/mystream2.png)

We can see that the `stream` structure is used for storing data, `rax` points to a radix tree which stores first unread ID as entry and the `keys` and `values` in the according `listpack` associated with key node in `rax`

## listpack

## rax

# read more
* [Introduction to Redis Streams](https://redis.io/topics/streams-intro)
