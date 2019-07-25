# hash

# contents

* [related file](#related-file)
* [encoding](#encoding)
	* [OBJ_ENCODING_ZIPLIST](#OBJ_ENCODING_ZIPLIST)
	* [OBJ_ENCODING_HT](#OBJ_ENCODING_HT)
* [read more](#read-more)

# related file
* redis/src/ziplist.h
* redis/src/ziplist.c
* redis/src/dict.h
* redis/src/dict.c

# encoding

## OBJ_ENCODING_ZIPLIST

> The ziplist is a specially encoded dually linked list that is designed to be very memory efficient. It stores both strings and integer values, where integers are encoded as actual integers instead of a series of characters. It allows push and pop operations on either side of the list in O(1) time. However, because every operation requires a reallocation of the memory used by the ziplist, the actual complexity is related to the amount of memory used by the ziplist.

from `redis/src/ziplist.c`

This is the overall layout of **ziplist**

![ziplist_overall_layout](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/ziplist_overall_layout.png)

Let's see a simple example first

    127.0.0.1:6379> HSET AA key1 33
    (integer) 1
    127.0.0.1:6379> OBJECT ENCODING AA
    "ziplist"

![simple_hash](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/simple_hash.png)

As far as we can learn from the above picture

`zlbytes` is the total bytes the **ziplist** occupy

`zltail` is the offset from current field to `zlend`

`zllen` indicates how many entries current **ziplist** have

# read more
