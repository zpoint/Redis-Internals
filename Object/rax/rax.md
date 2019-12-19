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

![rax](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/rax/rax.png)

# internal


    127.0.0.1:6379> xadd mystream * key1 val1
    "1575180011273-0"
    127.0.0.1:6379> XGROUP CREATE mystream mygroup1 $
    OK

Usually, this is what radix tree looks like

     * This is the vanilla representation:
     *
     *              (f) ""
     *                \
     *                (o) "f"
     *                  \
     *                  (o) "fo"
     *                    \
     *                  [t   b] "foo"
     *                  /     \
     *         "foot" (e)     (a) "foob"
     *                /         \
     *      "foote" (r)         (r) "fooba"
     *              /             \
     *    "footer" []             [] "foobar"
     *

From `redis/src/rax.h`

>  However, this implementation implements a very common optimization where successive nodes having a single child are "compressed" into the node itself as a string of characters, each representing a next-level child, and only the link to the node representing the last character node is provided inside the representation. So the above representation is turend into:

     *
     *                  ["foo"] ""
     *                     |
     *                  [t   b] "foo"
     *                  /     \
     *        "foot" ("er")    ("ar") "foob"
     *                 /          \
     *       "footer" []          [] "foobar"

The string `mygroup1` stored inside the rax is compressed, and can be described as

     *
     *                  ["mygroup1"] ""
     *                     |
     *                    [] "mygroup1"

The first node stored the compressed text `mygroup1`, the second node is an empty leaf, and is also a key, indicate that the string before this node is stored inside this `rax`

This is the actual layout of string `mygroup1`, and the `consumer group` object associate with the string `mygroup1` is pointed to by the data field of key node in the compressed radix tree

`iskey` indicates if this node contain a key

`isnull` means whether the associated value is NULL

`iscompr` means whether the current node is compressed(aforementioned)

`size` is the number of children or the compressed string length

![mygroup1](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/rax/mygroup1.png)

The first node with `iscompr` flag set, the whole string `mygroup1` is stored inside this single node, `size` is 8 means there're 8 `char` in the data field of current node, `iskey` is not set, means that current node only stores some data, we need to keep searching to find a key node

The second node with `iskey` set means the current node is a key node, it verify that the string before current node(which is `mygroup1`) is actually a valid string stored inside this radix tree, `isnull` is not set, which means there stores some data at the tail of data field, in this case it's a pointer to `streamCG` object, but it can actually be any other pointer, such as cluster key

Let's insert one more `consumer group` name to the current radix tree

    127.0.0.1:6379> XGROUP CREATE mystream mygroup2 $
    OK

![mygroup2](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/rax/mygroup2.png)

We can learn from the diagram that the first node is trimed and a new rax node is inserted, the bottom left node is the origin bottom node, the bottom right node is also a newly inserted node

     *
     *                  ["mygroup"] ""
     *                     |
     *                  [1   2] "mygroup"
     *                  /     \
     *      "mygroup1" []     [] "mygroup2"

The middle node is not `compressed`(`iscompr` not set), there will be `size` characters in the data field, and after these characters, there will also be `size` pointers to each `raxNode` structure

The two bottom nodes both labeled `iskey = 1`  and `isnull = 0`, it means when reach this node, the current string exists in this radix tree, and therr exists associated data for this key, the data is stored as a pointer in the rear of data field, How to represent the pointer depends on the caller

If you're interested in the insert and remove algorithm in the rax, please refer to the source code

`raxGenericInsert` in `redis/src/rax.c` and `raxRemove` in `redis/src/rax.c`

# read more

* [radix tree(wikipedia)](https://en.wikipedia.org/wiki/Radix_tree)
* [Rax, an ANSI C radix tree implementation](https://github.com/antirez/rax)
