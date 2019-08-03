# list

# contents

* [related file](#related-file)
* [memory layout](#memory-layout)
* [encoding](#encoding)
    * [OBJ_ENCODING_QUICKLIST](#OBJ_ENCODING_QUICKLIST)
* [read more](#read-more)


# related file
* redis/src/t_list.c
* redis/src/quicklist.h
* redis/src/quicklist.c

# memory layout

this is the layout of `quicklist`

![quicklist](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/list/quicklist.png)

# encoding

Previous redis version use `ziplist` for small amount of data, and `linked list` for list with size larger than certain amount of data, the encodings are **OBJ_ENCODING_ZIPLIST** and **OBJ_ENCODING_LINKEDLIST**, the upgrade strategy is similiar to the [upgrade in hash](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/hash.md#upgrade)

The current redis version no longer use `ziplist` or `linked list` to represent `list`, a new data structure named `quicklist` is used to achieve higher performance

> Summary of Recap: Quicklist is a linked list of ziplists. Quicklist combines the memory efficiency of small ziplists with the extensibility of a linked list allowing us to create space-efficient lists of any length.

from [redis quicklist visions](https://matt.sh/redis-quicklist-visions)

## OBJ_ENCODING_QUICKLIST

    127.0.0.1:6379> lpush lst 123 456
    (integer) 2
    127.0.0.1:6379> object encoding lst
    "quicklist"

![example](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/list/example.png)

We can learn from the above graph that **quicklist** stores a double linked list, each node in the double linked list is **quicklistNode**, field `zl` in **quicklistNode** points to a **ziplist**, **ziplist** is used and introduced in [hash(ziplist)](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/hash.md#OBJ_ENCODING_ZIPLIST)

# read more
* [redis quicklist](https://matt.sh/redis-quicklist)
* [redis quicklist visions](https://matt.sh/redis-quicklist-visions)
