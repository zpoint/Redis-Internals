# listpack

# contents

* [related file](#related-file)
* [memory layout](#memory-layout)
* [internal](#internal)
* [read more](#read-more)



# related file
* redis/src/listpack.c
* redis/src/listpack.h
* redis/src/util.c
* redis/src/util.h

# memory layout


![listpack](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/listpack/listpack.png)

# internal


    127.0.0.1:6379> xadd mystream * key1 val1
    "1575180011273-0"
    127.0.0.1:6379> XGROUP CREATE mystream mygroup1 $
    OK


# read more

[radix tree(wikipedia)](https://en.wikipedia.org/wiki/Radix_tree)

