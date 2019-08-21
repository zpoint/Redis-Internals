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


## OBJ_ENCODING_SKIPLIST

This is the layout of **zset**

![zskiplist](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/zset/zskiplist.png)

This is the layout of **skiplist**

![skiplist](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/zset/skiplist.png)

We can learn from the above picture that **zset** will store both [hash table](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/hash.md#OBJ_ENCODING_HT) and **skiplist** for indexing

I've set this line `zset-max-ziplist-entries 0` in my configure file, if you need information about the meaning of this line, please refer to [upgrade in hash](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/hash.md#upgrade)

    127.0.0.1:6379> zadd zset1 -1 val1 -3 val2 5 val3 8 val4 9 val5 7 val6 6 val7
    (integer) 7

A [Power law](https://en.wikipedia.org/wiki/Power_law) like function is used for generating the level, value of `ZSKIPLIST_P` is `0.25`, The probability of level 2 is `3/4`, level 3 is `1/4 ^ 2`, and so on

![power_law](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/zset/power_law.png)

There are less probability to get higher level and high probability to get lower level

Value in `ZSKIPLIST_MAXLEVEL` is 64, it means there can't be more than 64 levels


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


I've set `ZSKIPLIST_P` to a lower value `0.7` so that it's more likely to generate higher level(only for illustration purpose)

![skiplist1](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/zset/skiplist1.png)



