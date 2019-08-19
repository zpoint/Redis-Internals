# zset

# contents

* [prerequisites](#prerequisites)
* [related file](#related-file)
* [encoding](#encoding)
	* [OBJ_ENCODING_ZIPLIST](#OBJ_ENCODING_ZIPLIST)
	* [OBJ_ENCODING_SKIPLIST](#OBJ_ENCODING_SKIPLIST)

# prerequisites

* [hashtable in redis hash](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/hash.md#OBJ_ENCODING_HT)
* [ziplist in redis hash](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/hash.md#OBJ_ENCODING_ZIPLIST)

# related file
* redis/src/server.h
* redis/src/t_zset.c

# encoding

## OBJ_ENCODING_ZIPLIST

This is the layout of **skiplist**

![skiplist](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/zset/skiplist.png)

## OBJ_ENCODING_SKIPLIST

