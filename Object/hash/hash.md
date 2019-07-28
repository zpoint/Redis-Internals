# hash

# contents

* [related file](#related-file)
* [encoding](#encoding)
	* [OBJ_ENCODING_ZIPLIST](#OBJ_ENCODING_ZIPLIST)
		* [entry](#entry)
			* [prevlen](#prevlen)
			* [encoding](#encoding)
			* [entry data](#entry-data)
	* [OBJ_ENCODING_HT](#OBJ_ENCODING_HT)

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

Let's see a simple example

    127.0.0.1:6379> HSET AA key1 33
    (integer) 1
    127.0.0.1:6379> OBJECT ENCODING AA
    "ziplist"

![simple_hash](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/simple_hash.png)

As far as we can learn from the above picture

`zlbytes` is the total bytes the **ziplist** occupy

`zltail` is the offset from current field to `zlend`

`zllen` indicates how many entries current **ziplist** have

`zlend` is a one byte end mark of the **ziplist**

This is the layout of the two entry

![simple_hash_two_entry](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/simple_hash_two_entry.png)

`prevlen` stores the length of the previous entry, so that you are able to traverse backward, it's either 1 byte represent length as `uint8_t` which is less than 254 or 5 bytes, 1 for mark, the rest 4 can be represent length as `uint32_t`

`encoding` indicate how contents are encoded in the entry, there are various `encoding` for an `entry`, we will see later

### entry

we now know that entry consists of three parts

#### prevlen

`prevlen` is either 5 bytes or 1 byte, if the first byte of `prevlen` is 254, the following 4 bytes will be an unsigned integer value represent the length

![prevlen](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/prevlen.png)

#### encoding

the representation of the `entry data` depends on the `encoding`, and there are various kinds of `encoding`

![encoding](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/encoding.png)

#### entry data

`entry data` stores the actual data, it's either byte buffer or integer value, the actual representation depends on the `encoding` field

## OBJ_ENCODING_HT

