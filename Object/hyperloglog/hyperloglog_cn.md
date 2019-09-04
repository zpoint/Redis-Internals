# hyperloglog

# 目录

* [需要提前了解的知识](#需要提前了解的知识)
* [相关位置文件](#相关位置文件)
* [内存构造](#内存构造)
* [sparse](#sparse)
* [dense](#dense)
* [raw](#raw)
* [PFADD](#PFADD)
* [PFCOUNT](#PFCOUNT)
* [更多资料](#更多资料)

# 需要提前了解的知识

* [redis 字符串对象中的 sds 实现](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/sds/sds_cn.md)

# 相关位置文件
* redis/src/hyperloglog.c

# 内存构造

这是 **hyperloglog** 这个结构的内存构造

![hyperloglog](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hyperloglog/hyperloglog.png)

对于 **hyperloglog** 这个结构, 总共有 3 种不同的编码(存储)方式

    127.0.0.1:6379> PFADD key1 hello
    (integer) 1
    127.0.0.1:6379> OBJECT ENCODING key1
    "raw"

![sparse](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hyperloglog/sparse.png)

我们可以发现, **redis** 使用了 [sds](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/sds/sds_cn.md) 结构来存储 **hyperloglog**, **sds** 中的 `buf` 字段可以直接转换成  `hllhdr` 指针, `magic` 字段是一个字符串常量, 里面的值是 `HYLL`, 占用一字节大小的 `encoding` 表示了这个 **hyperloglog** 是以 **dense**(稠密) 的方式存储的还是以稀疏的方式存储的

    /* redis/src/hyperloglog.c */
    #define HLL_DENSE 0 /* 稠密. */
    #define HLL_SPARSE 1 /* 稀疏. */
    #define HLL_RAW 255 /* 只在内部处理的时候用到, 用户在外部无法操作 */

当前的 `encoding` 是 `HLL_SPARSE`

`card` 的作用是缓存 `PFCOUNT` 的结果, 避免每次调用都进行重复计算, 我们后面会看到示例

`registers` 存储了实际的数据

# sparse

大概 2 年前在处理一个 NLP 项目时第一次碰到 **sparse** 这个概念, 一个 NLP 项目中使用上了 [scipy 中的稀疏矩阵(csr_matrix)](https://docs.scipy.org/doc/scipy/reference/generated/scipy.sparse.csr_matrix.html), 当时看了很久的官方文档不得其意, 后在一个专家同事的帮助下才弄清了文档中稀疏矩阵中的 **rows** 和 **cols** 等参数值的含义

默认情况下当 **hyperloglog** 中存储的元素个数小于一定的值的时候, **redis** 会以稀疏的方式来存储 **hyperloglog** 这个结构

当你调用 `PFCOUNT` 并且进行真正的计算之后, `card` 字段会缓存 `PFCOUNT` 的结果, `card` 是以小端的方式存储, 权重最高的的一个 `bit` 用来表示当前 `card` 中缓存的计算结果是否已失效

![sparse_example](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hyperloglog/sparse_example.png)

当前在 `registers` 中的字节可以翻译成

	XZERO:9216
    VAL:1,1
    XZERO:7167

同样可以翻译成

	/* 总共有 16384 个 registers, 下标从 0 到 16383 */
    Registers 0-9215 的值为 0 (XZERO:9216)
    有一个 register 的值为 1, 这个 register 的下标为 9216 (VAL:1,1)
    XZERO:7167 (下标为 9217-16383 的 registers 都设置为 0) /* 9216 + 7167 = 16383 */

这是翻译并展开后的 registers 的样子, 如果一共有 16384 个 registers 并且每一个占用 `6 bits` 的大小来存储这个 register 对应的值, 那么一共需要 `16384 * 6(bits) = 12(kbytes)`(12kb 的大小来存储仅仅1个值为1的整型)

![sparse_full_registers](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hyperloglog/sparse_full_registers.png)

但是通过观察我们发现在 **sparse**(稀疏) 表示方式下, 实际上只用了 5 个字节就把对应的数据完整的存储了下来

![sparse_low_registers](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hyperloglog/sparse_low_registers.png)

    127.0.0.1:6379> PFADD key1 world
    (integer) 1
    127.0.0.1:6379> PFCOUNT key1
    (integer) 2

现在 `registers` 中的字节可以翻译成

	XZERO:2742
    VAL:3,1
    XZERO:6473
    VAL:1,1
    XZERO:7167

同样可以翻译成

    Registers 0-2741 的值为 0 (XZERO:2742)
	有一个 register 的值为 3, 这个 register 的下标为 2742 (VAL:3,1)
    XZERO:6473 (下标为 2743-9215 的 registers 都设置为 0) /* 2742 + 6473 = 9215 */
    有一个 register 的值为 1, 这个 register 的下标为 9216 (VAL:1,1)
    XZERO:7167 (下标为 9217-16383 的 registers 都设置为 0) /* 9216 + 7167 = 16383 */

通过稀疏的方式, 总共只用了 8 个字节就存储下了一整个长度为 16384 的数组

![sparse_low_registers2](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hyperloglog/sparse_low_registers2.png)

# dense

在 `redis.conf` 中有一个可配置的参数 `hll-sparse-max-bytes 3000`, 如果当前的 [sds](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/sds/sds_cn.md) 占用了超过 `hll-sparse-max-bytes` 大小的字节数, 那么 **sparse**(稀疏) 的表示方式就会被转换为 **dense**(稠密) 的方式

同样的存在一个无法配置的阈值 `#define HLL_SPARSE_VAL_MAX_VALUE 32`, 它表示如果当前计算出的 `count` 的值超过 32 时, 这个稀疏的结构也会被转换为稠密的结构

我在我的配置文件里设置了如下的配置 `hll-sparse-max-bytes 0`(仅仅是出于演示目的)

    127.0.0.1:6379> PFADD key1 here
    (integer) 1
    127.0.0.1:6379> PFCOUNT key1
    (integer) 3

现在 **hyperloglog** 的实际构造变成了如下的样子

![dense](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hyperloglog/dense.png)

完整的申请了 16384 个 registers 的空间, 每一个占用 `6 bits` 的大小

# raw

> hllCount() 支持一种特殊的, 只在程序内部使用的 encoding, 叫做 HLL_RAW, 意思是 hdr->registers 会指向一个 uint8_t 数组, 数组中每一个元素都对应一个 HLL_REGISTERS, 这么做可以在当 PFCOUNT 命令有多个 key 时加速计算速度(不需要再额外处理 6-bit 一个单位的整型)

这个 encoding 只在 `PFCOUNT` 命令中合并计算多个 key 的时候使用

# PFADD

**PFADD** 命令会使用 [murmurhash](https://en.wikipedia.org/wiki/MurmurHash) 函数对对应的 [sds](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/sds/sds_cn.md) 参数生成一个 64 bits 大小的的值, 对于这个值, 取最右边的 14 个 bits 作为 16384 个 registers 的位置下标, 剩下的 50 个 bits, 从最右边到最左边第一个出现 1 的位置记作 `count`, 并把这个 `count` 的值存储到 `registers[index]` 中

比如你调用如下命令时

	PFADD key1 hello

这是我的机器(小端)上的实际的 `murmur(hello)` 之后的值

![pfadd](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hyperloglog/pfadd.png)

我们把它表示成一个读起来更顺畅的形式(大端), 这样理解起来更方便

最右边的 14 个 bits 代表了一个无符号的整型, 这个整型是 `registers` 的下标位置

`count` 中的值会是 14th 到 63th 中第一个出现二进制 `1` 的位置, 比如 14th 上的值为 1, 那么 `count` 为 1， 如果一直往左第一个 1 在 18th 上, 那么 `count` 的值为 5

![pfadd2](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hyperloglog/pfadd2.png)

整个 `registers` 数组最终的结果如下图所示, 实际上真实的存储方式可能是 [稀疏](#dense) 的或者[稠密](#sparse) 的

![pfadd_full_registers](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hyperloglog/pfadd_full_registers.png)

如果你继续调用

	PFADD key1 world

`murmur(world)` 的结果如下图所示, 最右边 14 个 bits 的值为 2742, 并且从 14th 到最左边的 bit 中, 第一个 1 出现在 16th 的位置, 所以 `count` 的值为 3

![pfadd3](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hyperloglog/pfadd3.png)

这是 `PFADD` 命令执行后的 `registers` 数组

![pfadd_full_registers2](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hyperloglog/pfadd_full_registers2.png)

# PFCOUNT

`PFCOUNT` 的第一步是根据不同的 `encoding` 把所有的 `registers` 展开成完整的数组, 第二步根据 [New cardinality estimation algorithms for HyperLogLog sketches](https://arxiv.org/abs/1702.01284) 中的公式结合 `registers` 中存储的值来估算当前这个 key 的最终的值

基本的思路是通过第一个 1 出现的位置的最大值来预估判断这个集合里面总共存在了多少个不同的基数, 如果只有一个 `count` 值, 那么这个预估结果的误差会比较大, 所以取最右边的 14 个 bits 作为 `registers` 数组的下标位置, 这样总共有 16384 个 `count` 值, 通过所有分布在这 16384 个 `registers` 中的值可以平均求出一个误差不会太大的值

在计算完成后, 最终的值会被缓存在 `card` 字段中, 下一次调用 `PFCOUNT` 时会直接返回缓存在 `card` 字段中的值

    uint64_t hllCount(struct hllhdr *hdr, int *invalid) {
        double m = HLL_REGISTERS;
        double E;
        int j;
        int reghisto[64] = {0};

        /* 展开 register 数组 */
        if (hdr->encoding == HLL_DENSE) {
            hllDenseRegHisto(hdr->registers,reghisto);
        } else if (hdr->encoding == HLL_SPARSE) {
            hllSparseRegHisto(hdr->registers,
                             sdslen((sds)hdr)-HLL_HDR_SIZE,invalid,reghisto);
        } else if (hdr->encoding == HLL_RAW) {
            hllRawRegHisto(hdr->registers,reghisto);
        } else {
            serverPanic("Unknown HyperLogLog encoding in hllCount()");
        }

        /* 根据下面这篇论文的公式计算基数值
         * "New cardinality estimation algorithms for HyperLogLog sketches"
         * Otmar Ertl, arXiv:1702.01284 */
        double z = m * hllTau((m-reghisto[HLL_Q+1])/(double)m);
        for (j = HLL_Q; j >= 1; --j) {
            z += reghisto[j];
            z *= 0.5;
        }
        z += m * hllSigma(reghisto[0]/(double)m);
        E = llroundl(HLL_ALPHA_INF*m*m/z);

        return (uint64_t) E;
    }

# 更多资料
* [redis源码分析2--hyperloglog 基数统计](https://www.cnblogs.com/lh-ty/p/9972901.html)
* [New cardinality estimation algorithms for HyperLogLog sketches](https://arxiv.org/abs/1702.01284)