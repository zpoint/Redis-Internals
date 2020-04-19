# sds

# 目录

* [相关位置文件](#相关位置文件)
* [内存构造](#内存构造)
* [encoding](#encoding)
    * [OBJ_ENCODING_RAW](#OBJ_ENCODING_RAW)
        * [为什么 44 bytes](#why-44-bytes)
    * [OBJ_ENCODING_EMBSTR](#OBJ_ENCODING_EMBSTR)
    * [REDIS_ENCODING_INT](#REDIS_ENCODING_INT)
* [string header](#string-header)
    * [sdshdr5](#sdshdr5)
    * [sdshdr8](#sdshdr8)
    * [sdshdr16](#sdshdr16)
    * [sdshdr32](#sdshdr32)
    * [sdshdr64](#sdshdr64)
* [更多资料](#更多资料)

# 相关位置文件
* redis/src/sds.c
* redis/src/sds.h
* redis/src/sdsalloc.h
* redis/deps/hiredis/sds.h

# 内存构造

**sds** 是 **Simple Dynamic Strings** 的简称

> SDS 是 C 中的一个字符串相关的库, 设计这个库是为了增强 libc 对字符串相关的处理功能, 这个库支持从堆外内存申请分配字符串空间, 并且具备以下特性

> * 使用上非常简单
> * 二进制安全
> * 计算效率更高
> * 到目前为止和 C 的字符串处理函数兼容, 也就意味着你可以混用 libc 中的函数

对于这个库的设计和 API 相关说明感兴趣的同学请参考 [sds](https://github.com/antirez/sds)

根据不同的字符串长度, 实际上创建出来存储这个 sds 相关信息的 C 结构体是不一样的

![sds](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/sds/sds.png)

# encoding

**string** 这个类型在 redis 中总共有 3 种编码方式 **OBJ_ENCODING_RAW**, **OBJ_ENCODING_EMBSTR** and **REDIS_ENCODING_INT**

(这个 encoding 用来区分 redis 对象的编码方式, 几种不同的 sds 在 sds header 中区分, sds header 区分的是 sds 的存储方式, 他们是两个不同的概念)

```c
/* redis/src/object.c */
/* 选择 44 作为限制是因为在不到 44 个字符的时候, 当我们申请一个 EMBSTR 时最大需要申请的空间会小于 64 bytes
比一个 jemalloc 的 arena 单位要小 */
#define OBJ_ENCODING_EMBSTR_SIZE_LIMIT 44
robj *createStringObject(const char *ptr, size_t len) {
    if (len <= OBJ_ENCODING_EMBSTR_SIZE_LIMIT)
    	/* OBJ_ENCODING_EMBSTR */
        return createEmbeddedStringObject(ptr,len);
    else
    	/* OBJ_ENCODING_RAW */
        return createRawStringObject(ptr,len);
}

```

## OBJ_ENCODING_RAW

```shell script
127.0.0.1:6379> SET AA "hello world!"
OK
127.0.0.1:6379> OBJECT ENCODING AA
"embstr"

```

这是字符串对象 `AA` 的 C 结构体的内存构造

我们可以发现 `ptr` 指向了存储实际内容的 buffer, 这个 buffer 在这里是以 `\0` 结尾的 C 字符串

并且因为 buffer 中所存储的内容的长度在 44 字节以下, 这里的 encoding 是 `OBJ_ENCODING_EMBSTR`, 表示只需要做一次 **malloc** 系统调用即可, `robj` 和 `sdshdr8` 会存储在这一次申请回来的空间中, 他们的内存地址是连续的

**len** 存储了字符串对象的实际长度, 当前字符串对象 `hello world!` 的长度为 12

**alloc** 是 buffer 当中申请到的实际上的空间大小, buffer 的空间大小也许会比字符串的长度要长, 这样在进行字符串的扩展等操作时无需再进行 malloc/realloc 这个系统调用

![emb_str](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/sds/emb_str.png)

[C 中的 bit field](https://stackoverflow.com/questions/8564532/colon-in-c-struct-what-does-it-mean) 的实现方式取决于编译器和操作系统, 在我的机器上表示如下图所示

我的机器是小端机器, `type` 和 `encoding` 同时存在了第一个字节中, 并且 `lru` 占用了剩下的 24 bits, 他们合起来刚好是一个字长

![bit-field](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/sds/bit-field.png)

### 为什么 44 bytes

也许 redis 中存在如 [python 内存管理机制](https://github.com/zpoint/CPython-Internals/blob/master/Interpreter/memory_management/memory_management_cn.md) 类似的机制, 我后续会验证是否如此

最小的 **sds header** 占用的空间, 加上 **robj** 的空间和 44 个额外的字节的总和不到 64 个字节, 刚好能放在一个 jemalloc 中的 arena 中

## OBJ_ENCODING_EMBSTR

```shell script
127.0.0.1:6379> SET AA "this is a string longer than 44 characters!!!"
OK
127.0.0.1:6379> OBJECT ENCODING AA
"raw"

```

如果你创建了一个超过 44 字节的字符串对象, encoding 就会跟着一起改变

此时 **robj** 和 **sds** 不再是在连续的内存空间中, 至少需要调用 2 次 malloc 这个系统调用申请到这个字符串对象所需要的空间

![raw_str](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/sds/raw_str.png)

## REDIS_ENCODING_INT

```shell script
127.0.0.1:6379> SET AA 13
OK
127.0.0.1:6379> OBJECT ENCODING AA
"int"

```

此时的 **refcount** 为 **UINT_MAX**, **ptr** 存储的是真正的整数值, 以 **long** 的方式存储

![int_str](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/sds/int_str.png)

```c
/* redis/src/object.c */
len = sdslen(s);
if (len <= 20 && string2l(s,len,&value)) {
    /* 这个对象的编码方式是 long, 尝试使用公共的 long 对象而不是自己创建一个
       注意当设置了 maxmemory 时不会尝试使用公用对象
       因为每一个对象都会有一个自己的 LRU 字段才能使得 LRU 淘汰算法正常工作, 这些公有的对象会影响淘汰结果 */
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

```

从上面的代码我们可以看出, 当设置的字符串对象长度 <= 20 个字符时并且能成功转换为 long 值时, 会以 long 的方式存储在 **ptr** 中

我们来验证下上面的设想是否正确, `long` 在我的机器上是 64 个 bit 表示的, `1 << 63` 的值为 `9223372036854775808`, 因为 `long` 是一个有符号的值, 所以 `long` 能表示的最大的值为 `(1 << 63) - 1`

```shell script
127.0.0.1:6379> SET AA 9223372036854775806
OK
127.0.0.1:6379> OBJECT ENCODING AA
"int"
127.0.0.1:6379> INCR AA
(integer) 9223372036854775807

```

如果我们再增加一次, 就会产生上溢, 你需要用 `SET` 命令来设置这个新的值并且 redis 会以 `embstr` 的编码方式存储

```shell script
127.0.0.1:6379> INCR AA
(error) ERR increment or decrement would overflow
127.0.0.1:6379> SET AA 9223372036854775808
OK
127.0.0.1:6379> OBJECT ENCODING AA
"embstr"

```

# string header

根据字符串不同的实际长度, 会使用不同的 sds 类型

```c
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

```

## sdshdr5

通过 redis 客户端, 你没有办法设置 **sdshdr5** 这个字符串类型, 他把 **length** 和 **string header type** 一起压缩到了同一个 byte 中

![sdshdr5](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/sds/sdshdr5.png)

## sdshdr8

**sdshdr8** 用 **uint_8**(1 个字节) 来存储 **len** 和 **alloc**

```shell script
127.0.0.1:6379> SET AA "hello"
OK

```

![sdshdr8](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/sds/sdshdr8.png)

## sdshdr16

**sdshdr16** 用 **uint_16**(2 个字节) 来存储 **len** 和 **alloc**

![sdshdr16](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/sds/sdshdr16.png)

## sdshdr32

**sdshdr32** 用 **uint_32**(4 个字节) 来存储 **len** 和 **alloc**

![sdshdr32](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/sds/sdshdr32.png)

## sdshdr64

**sdshdr64** 只会在 64-bit 字长的机器上支持, **uint_64**(8 个字节) 用来存储 **len** 和 **alloc**

你能设置的最大的字符串对象的长度为 `(1 << 32) - 1`(32 位机器) 或 `(1 << 64) - 1`(64 位机器)

![sdshdr64](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/sds/sdshdr64.png)

# 更多资料
* [sds](https://github.com/antirez/sds)
