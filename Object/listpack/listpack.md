# listpack![image title](http://www.zpoint.xyz:8080/count/tag.svg?url=github%2FRedis-Internals%2Flistpack)

# contents

* [related file](#related-file)
* [ll2string](#ll2string)
* [memory layout](#memory-layout)
* [internal](#internal)
	* [overview](#overview)
	* [back length](#back-length)
	* [integer](#integer)
	* [string](#string)
* [read more](#read-more)

```c


```

# related file
* redis/src/listpack.c
* redis/src/listpack.h
* redis/src/util.c
* redis/src/util.h

# ll2string

I find an interesting algorithm to turn a `long long` value to a `string`(`char *` in `C`)

The concept and performance gain is fully explained in [Three Optimization Tips for C++](https://www.facebook.com/notes/facebook-engineering/three-optimization-tips-for-c/10151361643253920) and the source code of final version is used in `redis/src/util.c`

```c
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

```

It's a nearly binary search to count how many `100 digits` totally inside the 64 bit value

![digits10](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/listpack/digits10.png)

```c
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

```

For every vale after `% 100`, we search the table for the exact result for the current value, which is indexed in `2 * value`

![u64ToAsciiTable](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/listpack/u64ToAsciiTable.png)

# memory layout

> Listpack, suitable to store lists of string elements in a representation which is space efficient and that can be efficiently accessed from left to right and from right to left.

The first 6 bytes is a header named `LP_HDR_SIZE`, with the first 4 btytes store the total size in bytes of the current `listpack` in little endian order, which means the maximum size of a `listpack` you can create is about 4GB

The second 2 bytes stores how many elements currently stored inside the `listpack`, it's also in little endian order

![lp_hdr](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/listpack/lp_hdr.png)

# internal

## overview

Currently, the **listpack** is used in **stream** structure, let's create a stream and inspect the real layout of the internals(we only focus on the **listpack** part of **stream**)

```shell script
127.0.0.1:6379> xadd mystream * key1 128
"1576291564661-0"

```

![listpack1](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/listpack/listpack1.png)

```shell script
127.0.0.1:6379> xadd mystream * key1 val1
"1576291596105-0"

```

![listpack2](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/listpack/listpack2.png)

We can learn that it's a very compact list like data structure, We will leave what's the meaning of each element in the **listpack** to the other article [stream](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/stream/stream.md)

Currently, we just focus on how **listpack** stores different types of elements internally

## back length

Every element stores inside the **listpack** compose of two parts, the first part is the real element, the second part is the size the current elements occupies in bytes, so that you are able to traverse backward from the **listpack**, the actual size of **back length** varies depends on the number it represents

If **back length** is 1

![backlength_1](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/listpack/backlength_1.png)

If **back length** is 127

![backlength_127](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/listpack/backlength_127.png)

If **back length** is 128

![backlength_128](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/listpack/backlength_128.png)

If **back length** is 16384(actually the maximum number of bytes **back length** can occupy is 5)

![backlength_16384](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/listpack/backlength_16384.png)

We can learn that the **back length** is big endian, and only the right most 7 bits of each bytes is used, the left most bit is reserved, and the first byte's left most bit is always 0, the rest bytes' left most bit will always be 1

Why ?

Consider that when you traverse back from certain element, you need to read **back length** to know how many bytes previous element occupies, But don't know how many bytes the **back length** itself takes, so you've to read backward byte by byte, until some flag shows up and you can finish reading **back length**, compose the totally bytes you read to get the value of **back length**

The first bit of each byte in **back length** is this flag, 1 indicates the next(backward) byte is still part of **back length**, 0 indicate that the current byte is the final byte of **back length**, the next(backward) byte will be inside the actual element

```c
/* p is the first byte when you begin backward read "back length" */
uint64_t lpDecodeBacklen(unsigned char *p) {
    uint64_t val = 0;
    uint64_t shift = 0;
    do {
        val |= (uint64_t)(p[0] & 127) << shift;
        /* break if the left most bit is 1 */
        if (!(p[0] & 128)) break;
        /* continue if the left most bit is 0 */
        shift += 7;
        p--;
        if (shift > 28) return UINT64_MAX;
    } while(1);
    return val;
}

```

## integer

Besides the **back length**, the other component of each element is the real element, it can be encoded in two types, integer and string

When you type the **xadd** command, the **key** and **value** are both transmitted in **string** format, when storing inside the **listpack**, a function named `string2ll` will be called to try to convert the string to `int64_t` type

Storing elements inside the **listpack** also needs to store some related numeric value such as num fileds, when storing these num fields, `stream.insert` will use a signed version of [ll2string](#ll2string) aforementioned to convert it to string format, and `listpack.insert` will encode it back to integer format

```shell script
127.0.0.1:6379> xadd mystream * key1 128

```

After the conversion, the actual encoding varies

![LP_ENCODING_INT](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/listpack/LP_ENCODING_INT.png)

## string

If the conversion to string failed, the **string**(characters) will be stored inside directly, with some header bytes stores the length of the **string**

![LP_ENCODING_STR](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/listpack/LP_ENCODING_STR.png)

Let's see an example `key1`

![LP_ENCODING_STR_KEY1](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/listpack/LP_ENCODING_STR_KEY1.png)

Now, we understand the header of each element in the diagram in [overview](#overview)

# read more

* [Listpack README](https://github.com/antirez/listpack)
* [Three Optimization Tips for C++](https://www.facebook.com/notes/facebook-engineering/three-optimization-tips-for-c/10151361643253920)
