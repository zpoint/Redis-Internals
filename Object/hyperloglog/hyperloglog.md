# hyperloglog

# contents

* [related file](#related-file)
* [memory layout](#memory-layout)
* [sparse](#dense)
* [dense](#dense)
* [raw](#raw)

# related file
* redis/src/hyperloglog.c

# memory layout

This is the memory layout of a **hyperloglog** data structure

![hyperloglog](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hyperloglog/hyperloglog.png)

# sparse

There're totally three kinds of different encodings foe the **hyperloglog** data structure

    127.0.0.1:6379> PFADD key1 hello
    (integer) 1
    127.0.0.1:6379> OBJECT ENCODING key1
    "raw"

![sparse](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hyperloglog/sparse.png)

The first time I learn the concept **sparse** is [csr_matrix in scipy](https://docs.scipy.org/doc/scipy/reference/generated/scipy.sparse.csr_matrix.html) in a NLP project about 2 years ago, I can't figure out the meaning of **rows** and **cols** in **csr_matrix** by myself at that moment, with the help of a NLP expert college, I finallty figure out the meaning of official document

# dense

# raw