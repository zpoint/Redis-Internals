# sds

# contents

* [related file](#related-file)
* [memory layout](#memory-layout)
* [encoding](#encoding)
    * [OBJ_ENCODING_RAW](#OBJ_ENCODING_RAW)
        * [why 44 bytes](#why-44-bytes)
    * [OBJ_ENCODING_EMBSTR](#OBJ_ENCODING_EMBSTR)
    * [REDIS_ENCODING_INT](#REDIS_ENCODING_INT)
* [string header](#string-header)
    * [sdshdr5](#sdshdr5)
    * [sdshdr8](#sdshdr8)
    * [sdshdr16](#sdshdr16)
    * [sdshdr32](#sdshdr32)
    * [sdshdr64](#sdshdr64)
* [read more](#read-more)

# related file
* redis/src/sds.c
* redis/src/sds.h
* redis/src/sdsalloc.h
* redis/deps/hiredis/sds.h

# memory layout

**sds** is the abbreviation of the **Simple Dynamic Strings**

> SDS is a string library for C designed to augment the limited libc string handling functionalities by adding heap allocated strings that are:

> * Simpler to use.
> * Binary safe.
> * Computationally more efficient.
> * But yet... Compatible with normal C string functions.

for more introduction and API overview, please refer to [sds](https://github.com/antirez/sds)

There're various different layout depends on the actual string length

![sds](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/sds/sds.png)

For type **string**, there're totally three different encoding **OBJ_ENCODING_RAW**, **OBJ_ENCODING_EMBSTR** and **REDIS_ENCODING_INT**

	/* redis/src/object.c */
     * The current limit of 44 is chosen so that the biggest string object
     * we allocate as EMBSTR will still fit into the 64 byte arena of jemalloc. */
    #define OBJ_ENCODING_EMBSTR_SIZE_LIMIT 44
    robj *createStringObject(const char *ptr, size_t len) {
        if (len <= OBJ_ENCODING_EMBSTR_SIZE_LIMIT)
        	/* OBJ_ENCODING_EMBSTR */
            return createEmbeddedStringObject(ptr,len);
        else
        	/* OBJ_ENCODING_RAW */
            return createRawStringObject(ptr,len);
    }

# encoding

## OBJ_ENCODING_RAW

    127.0.0.1:6379> SET AA "hello world!"
    OK
    127.0.0.1:6379> OBJECT ENCODING AA
    "embstr"

This is the layout of the C structure of the string object `AA`

We can find that `ptr` will points to the beginning address of the buffer(here it's the null terminated C style string)

And because the length of the buffer is lower than 44, the encoding is `OBJ_ENCODING_EMBSTR`, it means only one **malloc** system call is called and the `robj` and `sdshdr8` will be in contiguously memory blocks

**len** is the length of the string, the current length of the `hello world!` is 12

**alloc** is the actual size of the buffer, the actual size of the buffer may be more than the length of the string so that you can resize the string without calling the malloc/realloc system call

![emb_str](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/sds/emb_str.png)

The low level detail of [bit field in C](https://stackoverflow.com/questions/8564532/colon-in-c-struct-what-does-it-mean) implement in my machine looks like the below picture

My machine is a little-endian machine, `type` and `encoding` live together inside the first byte, and `lru` takes the rest 24 bits, they fits into a single word size

![bit-field](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/sds/bit-field.png)

### why 44 bytes

there may exist [python like memory management mechanism](https://github.com/zpoint/CPython-Internals/blob/master/Interpreter/memory_management/memory_management.md) in redis, I will figure out later

the sum of smallest size of **sds header**, **robj** and 44 extra bytes is less than 64 bytes, it can fits into a single memory block

## OBJ_ENCODING_EMBSTR

    127.0.0.1:6379> SET AA "this is a string longer than 44 characters!!!"
    OK
    127.0.0.1:6379> OBJECT ENCODING AA
    "raw"

If you create a string with length greater than 44 bytes, the encoding will change

The address of **robj** and **sds** structure will not in contiguously memory any more, there're at least two malloc system call need to be called

![raw_str](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/sds/raw_str.png)

## REDIS_ENCODING_INT

    127.0.0.1:6379> SET AA 13
    OK
    127.0.0.1:6379> OBJECT ENCODING AA
    "int"

**refcount** of the int value is **UINT_MAX**, **ptr** stores the actual integer value, represent in **long** format

![int_str](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/sds/int_str.png)

	/* redis/src/object.c */
    len = sdslen(s);
    if (len <= 20 && string2l(s,len,&value)) {
        /* This object is encodable as a long. Try to use a shared object.
         * Note that we avoid using shared integers when maxmemory is used
         * because every object needs to have a private LRU field for the LRU
         * algorithm to work well. */
        if ((server.maxmemory == 0 ||
            !(server.maxmemory_policy & MAXMEMORY_FLAG_NO_SHARED_INTEGERS)) &&
            value >= 0 &&
            value < OBJ_SHARED_INTEGERS)
        {
            decrRefCount(o);
            incrRefCount(shared.integers[value]);
            return shared.integers[value];
        } else {
            if (o->encoding == OBJ_ENCODING_RAW) sdsfree(o->ptr);
            o->encoding = OBJ_ENCODING_INT;
            o->ptr = (void*) value;
            return o;
        }
    }

We can learn from the above source code that if the length is less or equal than 20 and it's convertible to a long value, the value will be represented and stored as a **long** value in **ptr**

Let's verify our assumption, `long` is 64 bits in my machine, `1 << 63` is `9223372036854775808`, because `long` is a signed type, the max value fits into `long` is `(1 << 63) - 1`

    127.0.0.1:6379> SET AA 9223372036854775806
    OK
    127.0.0.1:6379> OBJECT ENCODING AA
    "int"
    127.0.0.1:6379> INCR AA
    (integer) 9223372036854775807

If we increment the value again, it will overflow, you need to `SET` the new value and redis will encoded it as an `embstr`

    127.0.0.1:6379> INCR AA
    (error) ERR increment or decrement would overflow
    127.0.0.1:6379> SET AA 9223372036854775808
    OK
    127.0.0.1:6379> OBJECT ENCODING AA
    "embstr"

# string header

there're various different kinds of **string header type** depending on the length of the actual string

	redis/src/sds.c
    static inline char sdsReqType(size_t string_size) {
        if (string_size < 1<<5)
            return SDS_TYPE_5;
        if (string_size < 1<<8)
            return SDS_TYPE_8;
        if (string_size < 1<<16)
            return SDS_TYPE_16;
    #if (LONG_MAX == LLONG_MAX)
        if (string_size < 1ll<<32)
            return SDS_TYPE_32;
        return SDS_TYPE_64;
    #else
        return SDS_TYPE_32;
    #endif
    }

## sdshdr5

**sdshdr5** is never used by the user, it packs the **length** and **string header type** is the same byte

![sdshdr5](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/sds/sdshdr5.png)

## sdshdr8

**sdshdr5** use **uint_8**(1 byte) to store **len** and **alloc**

    127.0.0.1:6379> SET AA "hello"
    OK

![sdshdr8](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/sds/sdshdr8.png)

## sdshdr16

## sdshdr32

## sdshdr64

# read more
* [sds](https://github.com/antirez/sds)
