# sds

# contents

* [related file](#related-file)
* [memory layout](#memory-layout)
* [example](#example)
	* [OBJ_ENCODING_RAW](#OBJ_ENCODING_RAW)
	* [OBJ_ENCODING_EMBSTR](#OBJ_ENCODING_EMBSTR)
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
* Binary safe.
* Computationally more efficient.
* But yet... Compatible with normal C string functions.

for more introduction and API overview, please refer to [sds](https://github.com/antirez/sds)

There're various different layout depends on the actual string length

![sds](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/sds/sds.png)

# example

There're various different encoding depending on the type and the object stored inside the type

For type **string**, there're totally two different encoding **OBJ_ENCODING_RAW** and **OBJ_ENCODING_EMBSTR**

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

## OBJ_ENCODING_RAW

	127.0.0.1:6379> SET AA "hello world!"
	OK
    127.0.0.1:6379> GET AA
    "hello world!"

This is the layout of the C structure of the string object `AA`

We can find that `ptr` will points to the beginning address of the buffer(here it's the null terminated C style string)

And because the length of the buffer is lower than 44, the encoding is `OBJ_ENCODING_EMBSTR`, it means only one **malloc** system call is called and the `robj` and `sdshdr8` will be in contiguously memory blocks

**len** is the length of the string, the current length of the `hello world!` is 12

**alloc** is the actual size of the buffer, the actual size of the buffer may be more than the length of the string so that you can resize the string without calling the malloc/realloc system call

![emb_str](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/sds/emb_str.png)





# read more
* [sds](https://github.com/antirez/sds)
