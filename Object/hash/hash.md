# hash![image title](http://www.zpoint.xyz:8080/count/tag.svg?url=github%2FRedis-Internals%2Fhash)

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
		* [hash collisions](#hash-collisions)
		* [resize](#resize)
		* [activerehashing](#activerehashing)

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

```shell script
127.0.0.1:6379> HSET AA key1 33
(integer) 1
127.0.0.1:6379> OBJECT ENCODING AA
"ziplist"

```

![simple_hash](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/simple_hash.png)

As far as we can learn from the above picture

`zlbytes` is the total bytes the **ziplist** occupy

`zltail` is the offset from current field to `zlend`

`zllen` indicates how many entries current **ziplist** have

`zlend` is a one byte end mark of the **ziplist**

This is the layout of the newly inserted two entry(`key1` and `33`)

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

```shell script
127.0.0.1:6379> HSET AA key1 val1
(integer) 1
127.0.0.1:6379> HSET AA key2 123
(integer) 1

```

![two_value_ziplist](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/two_value_ziplist.png)

#### read

```shell script
127.0.0.1:6379> HGET AA key2
"123"

```

The `hget` command will traverse the `ziplist`, extract the key according to the `encoding`, and check the key to see if it's a match, it's an O(n) linear serach

#### update

This is the update part of the code

```c
/* redis/src/t_hash.c */
/* Delete value */
zl = ziplistDelete(zl, &vptr);

/* Insert new value */
zl = ziplistInsert(zl, vptr, (unsigned char*)value, sdslen(value));

```

Let's see an example

```shell script

127.0.0.1:6379> HSET AA key1 val_new
(integer) 0

```

The delete operation will take place first

![delete_ziplist](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/delete_ziplist.png)

Then it comes to the insert operation

Insert operation will invoke `realloc` to extend the size of the `ziplist`, move all the `entry` after the inserted item backward and insert the item to specific `entry`

`realloc` will delegate to `zrealloc` defined in `redis/src/zmalloc.h`, we will learn that in other article

![update_ziplist](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/update_ziplist.png)

#### delete

Delete operation will only move all the memory after the current deleted item forward, because the `prevlen` stores either 1 byte or 5 bytes, the `CascadeUpdate` will only happen if the entry size before deleted can't be stored in 1 byte, and the current deleting item's size can be stored in 1 byte, so the next item's `prevlen` field need to be extended, there must be a cascade update operation

For example, if we delete the second entry in the following picture

![cascad](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/cascad.png)

After delete and move every memory blocks forward

![cascad_promotion1](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/cascad_promotion1.png)

The 1 byte `prevlen` can't stores the length of the previous `entry`, the `ziplist` needs to be `realloc` and `prevlen` will be extended

![cascad_promotion2](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/cascad_promotion2.png)

What if the length of the newly extended `entry` now becomes longer than 255 bytes? the `prevlen` of the next `entry` can't stores the length again, so we need to `realloc` again

![cascad_promotion3](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/cascad_promotion3.png)

If you have a `ziplist` of N entry, every entry after the deleted entry need to be checked if it needs to be extended

This is the so called **cascad update**

### upgrade

```c
/* redis/src/t_hash.c */
/* in hset */
for (i = start; i <= end; i++) {
    if (sdsEncodedObject(argv[i]) &&
        sdslen(argv[i]->ptr) > server.hash_max_ziplist_value)
    {
        hashTypeConvert(o, OBJ_ENCODING_HT);
        break;
    }
}
/* ... */
/* Check if the ziplist needs to be converted to a hash table */
if (hashTypeLength(o) > server.hash_max_ziplist_entries)
    hashTypeConvert(o, OBJ_ENCODING_HT);

```

if the inserted object in of type [sds](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/sds/sds.md) and sds length is longer than `hash-max-ziplist-value` (you can config it in configure file, default value is 64), the `ziplist` will be converted to hash table(`OBJ_ENCODING_HT`)

Or the length of the `ziplist` after the isertion is loger than `hash-max-ziplist-entries`(default 512), it will also be converted to hash table

## OBJ_ENCODING_HT

I've set this line `hash-max-ziplist-entries 0` in my configure file

```c
hset AA key1 12

```

this is the layout after set a value

![dict_layout](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/dict_layout.png)

### hash collisions

The [SipHash 1-2](https://en.wikipedia.org/wiki/SipHash)(defined in `redis/src/siphash.c`) is used for redis hash function instead of the [murmurhash](https://en.wikipedia.org/wiki/MurmurHash) used in the previous redis version

According to the annotation in the redis source code, the speed of **SipHash 1-2** is the same as **Murmurhash2**, but the **SipHash 2-4** will slow down redis by a 4% figure more or less

The seed of the hash function is initlalized in the redis server start up

```c
/* redis/src/server.c */
char hashseed[16];
getRandomHexChars(hashseed,sizeof(hashseed));
dictSetHashFunctionSeed((uint8_t*)hashseed);

```

Because the **hashseed** is random generated, you are not able to predict the index of hash table for different redis instance, or for the same redis server after restart

```c
hset AA zzz val2

```

CPython use a [probing algorithm for hash collisions](https://github.com/zpoint/CPython-Internals/blob/master/BasicObject/dict/dict.md#hash-collisions-and-delete), while redis use single linked list

If two keys hash into the same index, they are stored as a single linked list, the latest inserted item in is the front

![dict_collision](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/dict_collision.png)

```c
hdel AA zzz

```

The delete operation will find the specific entry in the hash table, and resize if necessary

![dict_delete](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/dict_delete.png)

### resize

The function `_dictExpandIfNeeded` will be called for every dictionary insertion

```c
/* redis/src/dict.c */
/* Expand the hash table if needed */
static int _dictExpandIfNeeded(dict *d)
{
    /* Incremental rehashing already in progress. Return. */
    if (dictIsRehashing(d)) return DICT_OK;

    /* If the hash table is empty expand it to the initial size. */
    if (d->ht[0].size == 0) return dictExpand(d, DICT_HT_INITIAL_SIZE);

    /* If we reached the 1:1 ratio, and we are allowed to resize the hash
     * table (global setting) or we should avoid it but the ratio between
     * elements/buckets is over the "safe" threshold, we resize doubling
     * the number of buckets. */
    if (d->ht[0].used >= d->ht[0].size &&
        (dict_can_resize ||
         d->ht[0].used/d->ht[0].size > dict_force_resize_ratio))
    {
        return dictExpand(d, d->ht[0].used*2);
    }
    return DICT_OK;
}

```

in order to not affect the server performance, **redis** implements the [incremental resizing](https://en.wikipedia.org/wiki/Hash_table#Incremental_resizing) strategy, the resize operation is not done immediately, it's happening step by step in every CRUD request

let's see

```shell script
127.0.0.1:6379> del AA
(integer) 1
127.0.0.1:6379> hset AA 1 2
(integer) 1
127.0.0.1:6379> hset AA 2 2
(integer) 1
127.0.0.1:6379> hset AA 3 2
(integer) 1
127.0.0.1:6379> hset AA 4 2
(integer) 1

```

![resize_before](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/resize_before.png)

```shell script

127.0.0.1:6379> hset AA 5 2
(integer) 1

```

This time we insert a new entry, the `_dictExpandIfNeeded` will be called and `d->ht[0].used >= d->ht[0].size` will become true, `dictExpand` simply created a new hash table with two times of the old size and stores the newly created hash table to `d->ht[1]`

```c
/* redis/src/dict.c */
/* Expand or create the hash table */
int dictExpand(dict *d, unsigned long size)
{
    /* the size is invalid if it is smaller than the number of
     * elements already inside the hash table */
    if (dictIsRehashing(d) || d->ht[0].used > size)
        return DICT_ERR;

    dictht n; /* the new hash table */
    unsigned long realsize = _dictNextPower(size);

    /* Rehashing to the same table size is not useful. */
    if (realsize == d->ht[0].size) return DICT_ERR;

    /* Allocate the new hash table and initialize all pointers to NULL */
    n.size = realsize;
    n.sizemask = realsize-1;
    n.table = zcalloc(realsize*sizeof(dictEntry*));
    n.used = 0;

    /* Is this the first initialization? If so it's not really a rehashing
     * we just set the first hash table so that it can accept keys. */
    if (d->ht[0].table == NULL) {
        d->ht[0] = n;
        return DICT_OK;
    }

    /* Prepare a second hash table for incremental rehashing */
    d->ht[1] = n;
    d->rehashidx = 0;
    return DICT_OK;
}

```

Because the `rehashidx` is not -1, the new entry will be inserted to `ht[1]`

![resize_middle](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/resize_middle.png)

If the `rehashidx` is not -1, every CRUD operation will invoke the `rehash` function

```shell script
127.0.0.1:6379> hget AA 5
"2"

```

You will notice that `rehashidx` now becomes 1, and the bucket in table index[1] is moved down to the second table

![resize_middle2](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/resize_middle2.png)

```shell script
127.0.0.1:6379> hget AA not_exist
(nil)

```

`rehashidx` now becomes 3

![resize_middle3](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/resize_middle3.png)

```shell script
127.0.0.1:6379> hget AA 5
"2"

```

if the bucket in index 3 is finished, the hash table is fully transfered, `rehashidx` is set to -1 again, old table will be freed

![resize_done](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/resize_done.png)

and new table now becomes `ht[0]`, the location of the two table will be exchanged

![resize_done_reverse](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/resize_done_reverse.png)

```c

/* redis/src/dict.c */
int dictRehash(dict *d, int n) {
    /* for every HGET command, n is 1 */
    int empty_visits = n*10; /* Max number of empty buckets to visit. */
    if (!dictIsRehashing(d)) return 0;

    while(n-- && d->ht[0].used != 0) {
        /* rehash n bucket */
        dictEntry *de, *nextde;

        /* Note that rehashidx can't overflow as we are sure there are more
         * elements because ht[0].used != 0 */
        assert(d->ht[0].size > (unsigned long)d->rehashidx);
        while(d->ht[0].table[d->rehashidx] == NULL) {
            /* skip the empty bucket */
            d->rehashidx++;
            if (--empty_visits == 0) return 1;
        }
        de = d->ht[0].table[d->rehashidx];
        /* Move all the keys in this bucket from the old to the new hash HT */
        while(de) {
            uint64_t h;

            nextde = de->next;
            /* Get the index in the new hash table */
            h = dictHashKey(d, de->key) & d->ht[1].sizemask;
            de->next = d->ht[1].table[h];
            d->ht[1].table[h] = de;
            d->ht[0].used--;
            d->ht[1].used++;
            de = nextde;
        }
        d->ht[0].table[d->rehashidx] = NULL;
        d->rehashidx++;
    }

    /* Check if we already rehashed the whole table... */
    if (d->ht[0].used == 0) {
        zfree(d->ht[0].table);
        d->ht[0] = d->ht[1];
        _dictReset(&d->ht[1]);
        d->rehashidx = -1;
        return 0;
    }

    /* More to rehash... */
    return 1;
}

```

### activerehashing

As the configure file says

> Active rehashing uses 1 millisecond every 100 milliseconds of CPU time in order to help rehashing the main Redis hash table (the one mapping top-level keys to values). The hash table implementation Redis uses (see dict.c) performs a lazy rehashing: the more operation you run into a hash table that is rehashing, the more rehashing "steps" are performed, so if the server is idle the rehashing is never complete and some more memory is used by the hash table.
>
> The default is to use this millisecond 10 times every second in order to actively rehash the main dictionaries, freeing memory when possible.
>
> If unsure: use "activerehashing no" if you have hard latency requirements and it is not a good thing in your environment that Redis can reply from time to time to queries with 2 milliseconds delay.
>
> use "activerehashing yes" if you don't have such hard requirements but want to free memory asap when possible.

Redis server use a hash table to store every key and value, as we learn from the above examples, the hash table won't take steps rehashing unless you keep searching/modifying the hash table, `activerehashing` is used in the server main loop to help with that

```c
/* redis/src/server.c */
/* Rehash */
if (server.activerehashing) {
    for (j = 0; j < dbs_per_call; j++) {
        /* try to use 1 millisecond of CPU time at every call of this function to perform some rehahsing */
        int work_done = incrementallyRehash(rehash_db);
        if (work_done) {
            /* If the function did some work, stop here, we'll do
             * more at the next cron loop. */
            break;
        } else {
            /* If this db didn't need rehash, we'll try the next one. */
            rehash_db++;
            rehash_db %= server.dbnum;
        }
    }
}

```

