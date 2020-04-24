# zset

# contents

* [prerequisites](#prerequisites)
* [related file](#related-file)
* [encoding](#encoding)
	* [OBJ_ENCODING_ZIPLIST](#OBJ_ENCODING_ZIPLIST)
	* [OBJ_ENCODING_SKIPLIST](#OBJ_ENCODING_SKIPLIST)

# prerequisites

* [hashtable in redis hash](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/hash.md#OBJ_ENCODING_HT)
* [ziplist in redis hash](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/hash.md#OBJ_ENCODING_ZIPLIST)

# related file
* redis/src/server.h
* redis/src/t_zset.c

# encoding

## OBJ_ENCODING_ZIPLIST

```shell script
127.0.0.1:6379> zadd zset1 5 val3 -1 val1 -3 val2
(integer) 3

```

![zset_ziplist](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/zset/zset_ziplist.png)

A [ziplist](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/hash.md#OBJ_ENCODING_ZIPLIST) is used for storing light weight **zset**, the **score** is next to **entry**, and they are stored in accending order sorted by **score**


The default parameters in configure file is

```c
zset-max-ziplist-entries 128
zset-max-ziplist-value 64

```

It means if you have more than 128 elements in your **zset** or the sds string length of any of the element is longer than 64, the **ziplist** will be upgraded to **skiplist**

## OBJ_ENCODING_SKIPLIST

This is the layout of **zset**

![zset](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/zset/zset.png)

This is the layout of **skiplist**

![skiplist](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/zset/skiplist.png)

We can learn from the above picture that **zset** will store both [hash table](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/hash.md#OBJ_ENCODING_HT) and **skiplist** for indexing

I've set this line `zset-max-ziplist-entries 0` in my configure file, if you need information about the meaning of this line, please refer to [upgrade in hash](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/hash.md#upgrade)

```shell script
127.0.0.1:6379> del zset1
(integer) 1
127.0.0.1:6379> zadd zset1 -1 val1 -3 val2 5 val3 8 val4 9 val5 7 val6 6 val7
(integer) 7

```

A [Power law](https://en.wikipedia.org/wiki/Power_law) like function is used for generating the level, value of `ZSKIPLIST_P` is `0.25`, The probability of level 2 is `3/4`, level 3 is `1/4 ^ 2`, and so on

![power_law](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/zset/power_law.png)

There are less probability to get higher level and high probability to get lower level

Value in `ZSKIPLIST_MAXLEVEL` is 64, it means there can't be more than 64 levels

```c

/* Returns a random level for the new skiplist node we are going to create.
 * The return value of this function is between 1 and ZSKIPLIST_MAXLEVEL
 * (both inclusive), with a powerlaw-alike distribution where higher
 * levels are less likely to be returned. */
int zslRandomLevel(void) {
    int level = 1;
    while ((random()&0xFFFF) < (ZSKIPLIST_P * 0xFFFF))
        level += 1;
    return (level<ZSKIPLIST_MAXLEVEL) ? level : ZSKIPLIST_MAXLEVEL;
}


```

I've set `ZSKIPLIST_P` to a lower value `0.7` so that it's more likely to generate higher level(only for illustration purpose)

![skiplist1](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/zset/skiplist1.png)

We can learn from the above picture that all the inserted values are sorted in ascending order by `score` field, and the type of `score` is `double`([IEEE 754](https://en.wikipedia.org/wiki/IEEE_754-1985)/[IEEE-754标准与浮点数运算](https://blog.csdn.net/m0_37972557/article/details/84594879)), you can't set a value that overflow type `double`

The [`skiplist`](https://en.wikipedia.org/wiki/Skip_list) data structure allows you to search from the top level to bottom level, which results a O(log(N)) search complexity

So that in some command(i.e `ZREMRANGEBYSCORE`) you are able to find values by score in log(N) instead of linear time

There also exists a [hashtable](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/hash.md#OBJ_ENCODING_HT) in **zset**

![zset_dict](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/zset/zset_dict.png)

The entry in hash table stores the address of **score** field, which is a pointer to a double

So that in some command(i.e `ZSCORE`) you are able to find value and score by key in O(1) in the **hash table** instead of log(N) in **skiplist**
