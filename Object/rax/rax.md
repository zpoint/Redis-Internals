# rax

# contents

* [related file](#related-file)
* [memory layout](#memory-layout)
* [internal](#internal)
* [read more](#read-more)



# related file
* redis/src/rax.h
* redis/src/rax.c

# memory layout

**rax** is a redis implementation [radix tree](https://en.wikipedia.org/wiki/Radix_tree), it's a a space-optimized prefix tree which is used in many places in redis, such as storing `streams` consumer group and cluster keys


# internal

![rax](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/rax/rax.png)


    127.0.0.1:6379> xadd mystream * key1 val1
    "1575180011273-0"
    127.0.0.1:6379> XGROUP CREATE mystream mygroup1 $
    OK



    127.0.0.1:6379> XGROUP CREATE mystream mygroup2 $
    OK


# read more
[radix tree(wikipedia)](https://en.wikipedia.org/wiki/Radix_tree)
