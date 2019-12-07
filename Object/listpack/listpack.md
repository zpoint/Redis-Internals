# listpack

# contents

* [related file](#related-file)
* [ll2string](#ll2string)
* [memory layout](#memory-layout)
* [internal](#internal)
* [read more](#read-more)



# related file
* redis/src/listpack.c
* redis/src/listpack.h
* redis/src/util.c
* redis/src/util.h

# ll2string

I find an interesting algorithm to turn a `long long` value to a `string`(`char *` in `C`)

The concept and performance gain is fully explained in [Three Optimization Tips for C++](https://www.facebook.com/notes/facebook-engineering/three-optimization-tips-for-c/10151361643253920) and the source code of final version is used in `redis/src/util.c`

    /* P01 is 10, P02 is 10 * 10, P03 is 10 * 10 *10, ... etc
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

It's a nearly binary search to count how many `100 digits` totally inside the 64 bit value

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

        // Handle last 1-2 digits
        if (value < 10) {
            dst[next] = '0' + uint32_t(value);
        } else {
            auto i = uint32_t(value) * 2;
            dst[next] = digits[i + 1];
            dst[next - 1] = digits[i];
        }
        return length;
    }

For every vale after `% 100`, we search the table for the exact result for the current value, which is indexed in `2 * value`

![u64ToAsciiTable](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/listpack/u64ToAsciiTable.png)

# memory layout

> Listpack, suitable to store lists of string elements in a representation which is space efficient and that can be efficiently accessed from left to right and from right to left.


![listpack](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/listpack/listpack.png)

# internal


    127.0.0.1:6379> xadd mystream * key1 val1
    "1575180011273-0"
    127.0.0.1:6379> XGROUP CREATE mystream mygroup1 $
    OK


# read more

[Listpack README](https://github.com/antirez/listpack)
[Three Optimization Tips for C++](https://www.facebook.com/notes/facebook-engineering/three-optimization-tips-for-c/10151361643253920)
