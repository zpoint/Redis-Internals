# listpack

# 目录

* [相关位置文件](#相关位置文件)
* [ll2string](#ll2string)
* [内存构造](#内存构造)
* [内部实现](#内部实现)
	* [概览](#概览)
	* [back length](#back-length)
	* [integer](#integer)
	* [string](#string)
* [更多资料](#更多资料)



# 相关位置文件
* redis/src/listpack.c
* redis/src/listpack.h
* redis/src/util.c
* redis/src/util.h

# ll2string

我发现了一个有趣的算法, 它可以把 `long long` 类型转换为 `string` 类型(在 C 里面是 `char *` 数组)

算法的基本概念和性能的提升在这篇文章里已经详细的描述了 [Three Optimization Tips for C++](https://www.facebook.com/notes/facebook-engineering/three-optimization-tips-for-c/10151361643253920), 并且最终版被 redis 里的 `redis/src/util.c` 对应文件下的函数所采用并进行改进

    /* P01 是 10, P02 是 10 * 10, P03 是 10 * 10 *10, ... 以此类推
    uint32_t digits10(uint64_t v) {
        if (v < P01) return 1;
        if (v < P02) return 2;
        if (v < P03) return 3;
        if (v < P12) {
            if (v < P08) {
                if (v < P06) {
                    if (v < P04) return 4;
                    return 5 + (v >= P05);
                }
                return 7 + (v >= P07);
            }
            if (v < P10) {
                return 9 + (v >= P09);
            }
            return 11 + (v >= P11);
        }
        return 12 + digits10(v / P12);
    }

采用的是一种二分搜索的思想来统计在这个 64 bit 值里, 总共有多少个 100 的整数倍

![digits10](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/listpack/digits10.png)

    unsigned u64ToAsciiTable(uint64_t value, char* dst) {

        static const char digits[201] =
                "0001020304050607080910111213141516171819"
                "2021222324252627282930313233343536373839"
                "4041424344454647484950515253545556575859"
                "6061626364656667686970717273747576777879"
                "8081828384858687888990919293949596979899";

        uint32_t const length = digits10(value);
        uint32_t next = length - 1;
        while (value >= 100) {
            auto const i = (value % 100) * 2;
            value /= 100;
            dst[next] = digits[i + 1];
            dst[next - 1] = digits[i];
            next -= 2;
        }

        // 处理最后的 1-2 位
        if (value < 10) {
            dst[next] = '0' + uint32_t(value);
        } else {
            auto i = uint32_t(value) * 2;
            dst[next] = digits[i + 1];
            dst[next - 1] = digits[i];
        }
        return length;
    }

对于每一个 `% 100` 之后剩余的两位数, 我们在表里进行搜索, 找到对应的值, 他在表里的下标刚好是 `2 * value`

![u64ToAsciiTable](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/listpack/u64ToAsciiTable.png)

# 内存构造

> Listpack, 一个用来存储字符串元素的列表结构, 以非常节省空间的方式进行存储, 支持从左到右和从右到左获取元素

前 6 个字节叫做 `LP_HDR_SIZE`, 其中前 4 个字节存储了当前这个 `listpack` 总共的大小, 以小端的形式存储, 也就是说理论上你能创建的最大的 `listpack` 的大小为 4GB

后 2 个字节存储了当前的 `listpack` 中一共有多少个元素, 也是以小端形式存储的

![lp_hdr](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/listpack/lp_hdr.png)

# 内部实现

## 概览

当前, **stream** 对象中使用了 **listpack** 结构进行存储, 我们来创建一个 **stream** 并且看一下内部的真正结构(**stream** 本身也有结构, 并且也使用了其他的数据结构存储数据, 我们当前只关注 **listpack** 的这一部分)

    127.0.0.1:6379> xadd mystream * key1 128
    "1576291564661-0"

![listpack1](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/listpack/listpack1.png)

    127.0.0.1:6379> xadd mystream * key1 val1
    "1576291596105-0"

![listpack2](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/listpack/listpack2.png)

我们可以发现它是一个非常紧凑的数据结构, 我们会把每一个元素分别代表什么意思留到 [stream](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/stream/stream_cn.md) 这一篇文章中

当前我们关注点是 **listpack** 如何对不同类型和大小的元素进行编码和存储

## back length

**listpack** 中的每一个元素都由2部分组成, 第一部分是这个真正的元素, 第二部分是这个元素在当前的 **listpack** 中一共占用了多少字节, 通过这个第二部分, 你能对 **listpack** 进行倒序遍历, **back length** 的真正长度取决于它要表示的数的大小

如果 **back length** 长度为 1

![backlength_1](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/listpack/backlength_1.png)

如果 **back length** 长度为 127

![backlength_127](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/listpack/backlength_127.png)

如果 **back length** 长度为 128

![backlength_128](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/listpack/backlength_128.png)

如果 **back length** 长度为 16384(**back length** 本身最多能占用 5 个字节)

![backlength_16384](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/listpack/backlength_16384.png)

我们可以发现, **back length** 是以大端的形式进行存储的, 并且每个字节只有最右边的 7 个 bit 被用到了编码中, 最左边的 1 个字节是保留起来的, 其中, 第一个字节的最左边的保留位会是 0, 剩下的字节的最左边的保留位会是 1

为什么?

假设你在某个元素的位置上, 需要从后往前进行遍历, 你需要首先读取前一个元素的 **back length** 去知道前一个元素占用的字节大小, 但是你此时不知道 **back length** 本身占用了多少个字节, 你只能一个字节一个字节的往回读取, 直到某个标记位出现, 你知道这是 **back length** 的最后一个字节了, 把刚刚读取到的字节组合起来, 获得这个 **back length** 表示的真正大小

每一个 **back length** 中的第一个 bit 就是起到了这个标记的作用, 1 表示下一个字节(往前读取) 仍然是 **back length** 的一部分, 0 表示当前这个字节已经是 **back length** 的最后一个字节了, 下一个字节(往前读取) 会是实际元素中的字节

	/* p 是你开始往回读取的时候, 往左偏移一个字节的位置 */
    uint64_t lpDecodeBacklen(unsigned char *p) {
        uint64_t val = 0;
        uint64_t shift = 0;
        do {
            val |= (uint64_t)(p[0] & 127) << shift;
            /* 如果最左边的 bit 是 1, 跳出循环 */
            if (!(p[0] & 128)) break;
            /* 如果最左边的 bit 是 0, 继续 */
            shift += 7;
            p--;
            if (shift > 28) return UINT64_MAX;
        } while(1);
        return val;
    }

## integer

除了 **back length**, 另一部分就是这个真正的元素了, 这个元素可以以两种最基本的类型进行编码, 整数(integer)和字符串(string)

当你输入 **xadd** 命令时, **key** 和 **value** 都以 **string** 格式编码进行传输, 在存储到 **listpack** 的时候, 一个名为 `string2ll` 的函数会被调用, 尝试把这个字符串转换为 `int64_t` 类型, 在存储这个值相关联的整型(比如元素数量)的时候, `stream.insert` 会尝试用上述的 [ll2string](#ll2string) 把它转换为字符串, 然后 `listpack.insert` 再对字符串进行对应的编码, 把它转换回整型

	127.0.0.1:6379> xadd mystream * key1 128

在转换成整数之后, 真正的表示方式是比较多样的

![LP_ENCODING_INT](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/listpack/LP_ENCODING_INT.png)

## string

如果对这个元素转换成整数失败的话, 这整个 **string**(字符串) 会被直接存储到 **listpack** 中, 其中有些头部信息(**string**的编码方式和长度) 会插到这个字符串的头部

![LP_ENCODING_STR](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/listpack/LP_ENCODING_STR.png)

我们来看一下 `key1` 的示例

![LP_ENCODING_STR_KEY1](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/listpack/LP_ENCODING_STR_KEY1.png)

现在我们基本弄明白了 [overview](#overview) 几个图例中的各个元素的头部几个字节的意义啦

# 更多资料

* [Listpack README](https://github.com/antirez/listpack)
* [Three Optimization Tips for C++](https://www.facebook.com/notes/facebook-engineering/three-optimization-tips-for-c/10151361643253920)
