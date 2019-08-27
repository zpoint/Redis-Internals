# hyperloglog

# contents

* [prerequisites](#prerequisites)
* [related file](#related-file)
* [memory layout](#memory-layout)
* [sparse](#dense)
* [dense](#dense)
* [raw](#raw)

# prerequisites

* [sds in redis string](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/sds/sds.md)

# related file
* redis/src/hyperloglog.c

# memory layout

This is the memory layout of a **hyperloglog** data structure

![hyperloglog](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hyperloglog/hyperloglog.png)

There're totally three kinds of different encodings for the **hyperloglog** data structure

    127.0.0.1:6379> PFADD key1 hello
    (integer) 1
    127.0.0.1:6379> OBJECT ENCODING key1
    "raw"

![sparse](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hyperloglog/sparse.png)

The first time I learn the concept **sparse** is [csr_matrix in scipy](https://docs.scipy.org/doc/scipy/reference/generated/scipy.sparse.csr_matrix.html) in a NLP project about 2 years ago, I can't figure out the meaning of **rows** and **cols** in **csr_matrix** by myself at that moment, with the help of a NLP expert college, I finallty figure out the meaning of the document

We can learn that **redis** use [sds](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/sds/sds.md) for storing the **hyperloglog** data structure, `buf` field in the **sds** can be cast to `hllhdr` pointer directly, the `magic` is a string, it will always be `HYLL`, the one byte `encoding` indicating that whether this **hyperloglog** is a **dense** representation or **sparse** representation

    /* redis/src/hyperloglog.c */
    #define HLL_DENSE 0 /* Dense encoding. */
    #define HLL_SPARSE 1 /* Sparse encoding. */
    #define HLL_RAW 255 /* Only used internally, never exposed. */

The current `encoding` is `HLL_SPARSE`

`card` is used for caching the `PFCOUNT` result to avoid computing the value every time, we will see an example later

`registers` stores the real data

# sparse

# dense

# raw