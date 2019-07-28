# hash

# contents

* [related file](#related-file)
* [encoding](#encoding)
	* [OBJ_ENCODING_ZIPLIST](#OBJ_ENCODING_ZIPLIST)
		* [entry](#entry)
			* [prevlen](#prevlen)
			* [encoding](#encoding)
			* [entry data](#entry-data)
		* [CRUD operation](#CRUD-operation)
            * [create](#create)
            * [read](#read)
            * [update](#update)
            * [delete](#delete)
		* [upgrade](#upgrade)
	* [OBJ_ENCODING_HT](#OBJ_ENCODING_HT)

# related file
* redis/src/ziplist.h
* redis/src/ziplist.c
* redis/src/dict.h
* redis/src/dict.c

# encoding

## OBJ_ENCODING_ZIPLIST

> The ziplist is a specially encoded dually linked list that is designed to be very memory efficient. It stores both strings and integer values, where integers are encoded as actual integers instead of a series of characters. It allows push and pop operations on either side of the list in O(1) time. However, because every operation requires a reallocation of the memory used by the ziplist, the actual complexity is related to the amount of memory used by the ziplist.

From `redis/src/ziplist.c`

This is the overall layout of **ziplist**

![ziplist_overall_layout](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/ziplist_overall_layout.png)

Let's see a simple example

    127.0.0.1:6379> HSET AA key1 33
    (integer) 1
    127.0.0.1:6379> OBJECT ENCODING AA
    "ziplist"

![simple_hash](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/simple_hash.png)

As far as we can learn from the above picture

`zlbytes` is the total bytes the **ziplist** occupy

`zltail` is the offset from current field to `zlend`

`zllen` indicates how many entries current **ziplist** have

`zlend` is a one byte end mark of the **ziplist**

This is the layout of the two entry

![simple_hash_two_entry](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/simple_hash_two_entry.png)

`prevlen` stores the length of the previous entry, so that you are able to traverse backward, it's either 1 byte represent length as `uint8_t` which is less than 254 or 5 bytes, 1 for mark, the rest 4 can be represent length as `uint32_t`

`encoding` indicate how contents are encoded in the entry, there are various `encoding` for an `entry`, we will see later

### entry

We now know that entry consists of three parts

#### prevlen

`prevlen` is either 5 bytes or 1 byte, if the first byte of `prevlen` is 254, the following 4 bytes will be an unsigned integer value represent the length

![prevlen](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/prevlen.png)

#### encoding

The representation of the `entry data` depends on the `encoding`, and there are various kinds of `encoding`

![encoding](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/encoding.png)

#### entry data

`entry data` stores the actual data, it's either byte buffer or integer value, the actual representation depends on the `encoding` field

### CRUD operation

#### create

An empty `ziplist` will be created for the first time you call `hset` command with some key

![empty_ziplist](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/empty_ziplist.png)

    127.0.0.1:6379> HSET AA key1 val1
    (integer) 1
    127.0.0.1:6379> HSET AA key2 123
    (integer) 1

![two_value_ziplist](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/two_value_ziplist.png)

#### read

    127.0.0.1:6379> HGET AA key2
    "123"

The `hget` command will traverse the `ziplist`, extract the key according to the `encoding`, and check the key to see if it's a match, it a O(n) linear serach

#### update

This is the update part of the code

	/* redis/src/t_hash.c */
    /* Delete value */
    zl = ziplistDelete(zl, &vptr);

    /* Insert new value */
    zl = ziplistInsert(zl, vptr, (unsigned char*)value, sdslen(value));

Let's see an example


    127.0.0.1:6379> HSET AA key1 val_new
    (integer) 0

The delete operation will take place first

![delete_ziplist](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/delete_ziplist.png)

Then it comes to the insert operation

Insert operation will invoke `realloc` to extend the size of the `ziplist`, move all the `entry` after the inserted item backward and insert the item to specific `entry`

`realloc` will delegate to `zrealloc` defined in `redis/src/zmalloc.h`, we will learn that in other article

![update_ziplist](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/update_ziplist.png)

#### delete

Delete operation will only move all the memory after the current deleted item forward, because the `prevlen` stores either 1 byte or 5 bytes, the `CascadeUpdate` will only happen if the entry size before deleted can't be stored in 1 byte, and the current deleting item's size can be stored in 1 byte, so the next item's `prevlen` field need to be extended, there must be a cascade update operation

For example, if we delete the second entry in the following picture

![cascade](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/cascade.png)

After delete and move every memory blocks forward

![cascad_promotion1](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/cascad_promotion1.png)

The 1 byte `prevlen` can stores the length of the previous `entry`, the `ziplist` needs to be `realloc` and `prevlen` will be extended

![cascad_promotion2](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/cascad_promotion2.png)

What if the length of the newly extended `entry` now becomes longer than 255 bytes? the `prevlen` of the next `entry` can't stores the length again, so we need to `realloc` again

![cascad_promotion3](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/cascad_promotion3.png)

This is the so called **cascad update**

### upgrade

	/* redis/src/t_hash.c */
    for (i = start; i <= end; i++) {
        if (sdsEncodedObject(argv[i]) &&
            sdslen(argv[i]->ptr) > server.hash_max_ziplist_value)
        {
            hashTypeConvert(o, OBJ_ENCODING_HT);
            break;
        }
    }

if the inserted object in of type [sds](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/sds/sds.md) and sds length is longer than `hash_max_ziplist_value` (config in configure file, default value is 64), the `ziplist` will be converted to hash table(`OBJ_ENCODING_HT`)

## OBJ_ENCODING_HT



