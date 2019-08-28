# hyperloglog

# contents

* [prerequisites](#prerequisites)
* [related file](#related-file)
* [memory layout](#memory-layout)
* [sparse](#dense)
* [dense](#dense)
* [raw](#raw)
* [read more](#read-more)

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

We can learn that **redis** use [sds](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/sds/sds.md) for storing the **hyperloglog** data structure, `buf` field in the **sds** can be cast to `hllhdr` pointer directly, the `magic` is a string, it will always be `HYLL`, the one byte `encoding` indicating that whether this **hyperloglog** is a **dense** representation or **sparse** representation

    /* redis/src/hyperloglog.c */
    #define HLL_DENSE 0 /* Dense encoding. */
    #define HLL_SPARSE 1 /* Sparse encoding. */
    #define HLL_RAW 255 /* Only used internally, never exposed. */

The current `encoding` is `HLL_SPARSE`

`card` is used for caching the `PFCOUNT` result to avoid recomputing the value every time, we will see an example later

`registers` stores the real data

# sparse

The first time I learn the concept **sparse** is [csr_matrix in scipy](https://docs.scipy.org/doc/scipy/reference/generated/scipy.sparse.csr_matrix.html) in a NLP project about 2 years ago, I can't figure out the meaning of **rows** and **cols** in **csr_matrix** by myself at that moment, with the help of a NLP expert college, I finallty figure out the meaning of the document

By default **redis** represents **hyperloglog** in a **sparse** format when the number of entries stores inside the **hyperloglog** is less than a specific value

`card` will cache the `PFCOUNT` result whenever you call `PFCOUNT`, it's stored in little-endian, with the most significant bit indicates whether the current cache value is valid or not

![sparse_example](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hyperloglog/sparse_example.png)

Those bytes in `registers` fields will be represented as

	XZERO:9216
    VAL:1,1
    XZERO:7167

which can be translated to

	/* There're totally 16384 registers, index from 0 to 16383 */
	Registers 0-9215 are set to 0 (XZERO:9216)
    1 register set to value 1, that is register 9216 (VAL:1,1)
    XZERO:7167 (Registers 9217-16383 set to 0) /* 9216 + 7167 = 16383 */

This is the layout of the registers after translation, if there're totally 16384 registers and each takes 6 bits to stores an integer value, it will takes `16384 * 6(bits) = 12(kbytes)` (12kb to store just 1 single integer value 1)

![sparse_full_registers](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hyperloglog/sparse_full_registers.png)

But it turns out that only 5 bytes are used in the **sparse** representation

![sparse_low_registers](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hyperloglog/sparse_low_registers.png)

    127.0.0.1:6379> PFADD key1 world
    (integer) 1
    127.0.0.1:6379> PFCOUNT key1
    (integer) 2

Now bytes in `registers` fields will be represented as

	XZERO:2742
    VAL:3,1
    XZERO:6473
    VAL:1,1
    XZERO:7167

which can be translated to

	Registers 0-2741 are set to 0 (XZERO:2742)
    1 register set to value 3, that is register 2742 (VAL:3,1)
    XZERO:6473 (Registers 2743-9215 set to 0) /* 2742 + 6473 = 9215 */
    1 register set to value 1, that is register 9216 (VAL:1,1)
    XZERO:7167 (Registers 9217-16383 set to 0) /* 9216 + 7167 = 16383 */

It only takes 8 bytes to represent the 16384 registers in **sparse** way

![sparse_low_registers2](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hyperloglog/sparse_low_registers2.png)

# dense

There's a configurable threshold named `hll-sparse-max-bytes 3000` in the `redis.conf` configure file, if the [sds](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/sds/sds.md) occupies more than `hll-sparse-max-bytes`, the **sparse** structure will bo promoted to **dense** structure

There also exists an unconfigurable threshold defined as `#define HLL_SPARSE_VAL_MAX_VALUE 32`, it means if the `count` value in the current register can't fit into the **sparse** `6 bit` value, it will also be promoted

The **dense** representation will use `6 bits` for each register, `16384 registers` will cost `12(kbytes)`



# raw

# read more
* [redis源码分析2--hyperloglog 基数统计](https://www.cnblogs.com/lh-ty/p/9972901.html)