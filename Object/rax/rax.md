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

>  * However, this implementation implements a very common optimization where
>  * successive nodes having a single child are "compressed" into the node
>  * itself as a string of characters, each representing a next-level child,
>  * and only the link to the node representing the last character node is
>  * provided inside the representation. So the above representation is turend
>  * into:

     *
     *                  ["foo"] ""
     *                     |
     *                  [t   b] "foo"
     *                  /     \
     *        "foot" ("er")    ("ar") "foob"
     *                 /          \
     *       "footer" []          [] "foobar"

This is the actual layout of string `mygroup1`, and the `consumer group` object associate with the string `mygroup1` is pointed to by the key node in the compressed radix tree

`iskey` indicates if this node contain a key

`isnull` means whether the associated value is NULL

![mygroup1](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/rax/mygroup1.png)

The abstract diagram may be more easily understanding


    127.0.0.1:6379> XGROUP CREATE mystream mygroup2 $
    OK


# read more
[radix tree(wikipedia)](https://en.wikipedia.org/wiki/Radix_tree)
