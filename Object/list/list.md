# list

# contents

* [prerequisites](#prerequisites)
* [related file](#related-file)
* [memory layout](#memory-layout)
* [encoding](#encoding)
    * [OBJ_ENCODING_QUICKLIST](#OBJ_ENCODING_QUICKLIST)
    	* [quicklist](#quicklist)
    	* [quicklistNode](#quicklistNode)
    	* [example](#example)
    		* [list max ziplist size](#list-max-ziplist-size)
    		* [list compress depth](#list-max-ziplist-size)
    		* [insert](#insert)
    		* [delete](#delete)
* [read more](#read-more)

# prerequisites

* [ziplist in redis hash](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/hash.md#OBJ_ENCODING_ZIPLIST)

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

### quicklist

`head` and `tail` points the the head and tail part of the double linked list, the node **quicklist** stores is a struct named **quicklistNode**, if it's an empty quicklist, `head` and `tail` both points th `NULL` pointer, if the size of the double linked list is 1, `head` and `tail` both points to the same node

`count` is a counter indicate how many elements currently stored inside the `list`, it's type is `unsigned long`, so the maximum elements a `list` can stored is `(1 << 64) - 1` in `64 bit` word size machine and `(1 << 32) - 1` in `32 bit` word size machine

`len` indicates the length of the double linked list(number of quicklistNodes)

`fill` indicates the fill factor for individual nodes, what does that mean ?

> Lists are also encoded in a special way to save a lot of space. The number of entries allowed per internal list node can be specified as a fixed maximum size or a maximum number of elements. For a fixed maximum size, use -5 through -1, meaning:
> -5: max size: 64 Kb  <-- not recommended for normal workloads
> -4: max size: 32 Kb  <-- not recommended
> -3: max size: 16 Kb  <-- probably not recommended
> -2: max size: 8 Kb   <-- good
> -1: max size: 4 Kb   <-- good
> Positive numbers mean store up to _exactly_ that number of elements per list node.
> The highest performing option is usually -2 (8 Kb size) or -1 (4 Kb size),
> but if your use case is unique, adjust the settings as necessary.

from `redis.conf`, it's a configurable parameter `list-max-ziplist-size`, the default value is -2, it controls the value in `fill` field in the **quicklist** structure, negative value limits max size of **ziplist** in **quicklistNode**, while positive limits max elements of **ziplist** in **quicklistNode**

`compress` indicates how many node will be compressed excluding `compress` elements in each side in **ziplist** in **quicklistNode**

> Lists may also be compressed. Compress depth is the number of quicklist ziplist nodes from *each* side of the list to *exclude* from compression.  The head and tail of the list are always uncompressed for fast push/pop operations.  Settings are:
> 0: disable all list compression
> 1: depth 1 means "don't start compressing until after 1 node into the list,
>    going from either the head or tail"
>    So: [head]->node->node->...->node->[tail]
>    [head], [tail] will always be uncompressed; inner nodes will compress.
> 2: [head]->[next]->node->node->...->node->[prev]->[tail]
>    2 here means: don't compress head or head->next or tail->prev or tail,
>    but compress all nodes between them.
> 3: [head]->[next]->[next]->node->node->...->node->[prev]->[prev]->[tail]
> etc.

from `redis.conf`, it's configurable parameter named `list-compress-depth`, default value is 0(means no compression), redis use [LZF alfgorithm](https://github.com/ning/compress/wiki/LZFFormat) for compression, it's optimized for speed and low memory usage, we will see later

### quicklistNode

`prev` and `next` points the previous and next node of the current **quicklistNode**

`zl` points to the actual **ziplist**

`sz` stores the ziplist size in bytes

`count` indicates how many elements currently stored inside the **ziplist**, it's a 16 bit field so the maximum elements a **quicklistNode** can store is less than 65536

`encoding` indicates how elements in the **ziplist** are encoded, 1(RAW) means plain data, 2(LZF) means LZF algorithm compressed data

`container` indicates which data structure is used in the **quicklistNode**, the current support value is 2(ZIPLIST), there may be more data structure supported in the future

`recompress` is a 1 bit boolean value, it means whether the current node is temporarry decompressed for usage

`attempted_compress` is only used in test case

`extra` is the extra bits reserved for future use

### example

#### list max ziplist size

Let's set the folloing two lines in the configure file

	list-max-ziplist-size 3
    list-compress-depth 0

In the redis client command line

    127.0.0.1:6379> del lst
    (integer) 1
    127.0.0.1:6379> lpush lst val1 123 456
    (integer) 3

![max_size_1](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/list/max_size_1.png)

`fill`(maximum elements per node) is 3, `len`(length of nodes) is 1, `count`(total elements num) is 3

If we insert again

    127.0.0.1:6379> lpush lst 789
    (integer) 4

Because elements in the first node is greater or equal than `fill`, a new node will be created

`fill` stays 3, `len` becomes 2, `count` becomes 3

![max_size_2](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/list/max_size_2.png)

#### list compress depth



# read more
* [redis quicklist](https://matt.sh/redis-quicklist)
* [redis quicklist visions](https://matt.sh/redis-quicklist-visions)
* [Redis内部数据结构详解(5)——quicklist](http://zhangtielei.com/posts/blog-redis-quicklist.html)

