# persistence

# contents

* [related file](#related-file)
* [AOF](#AOF)
	* [rewrite](#rewrite)
* [RDB](#RDB)

# related file

* redis/src/aof.c
* redis/src/rdb.c
* redis/src/rdb.h
* redis/src/adlist.h
* redis/src/adlist.c

There're currently two types of persistence available in redis

# AOF

If we set `appendonly yes` in `redis.conf`

And in command line

    127.0.0.1:6379> set key1 val1
    OK

And we inspect the `appendonly` file

    cat appendonly.aof
    *2
    $6
    SELECT
    $1
    0
    *3
    $3
    set
    $4
    key1
    $4
    val1

We can find that the `aof` file stores commands one by one

symbol `*` follows the argument count

symbol `$` follows the next string length

Let's read my `appendonly.aof` as example

`*2` means the following command has 2 arguments

The first argument is `$6\r\n` follow by `SELECT\r\n`, it means the string length is 6, if you read the next 6 characters, `SELECT` is the target string with exact length 6

The second argument is `$1\r\n` follow by `0\r\n`, it means the string lengtg is 1, if you read the next 1 character, you can get `0`

Together, it's `SELECT 0`

Similarly, the next command is `set key1 val1`

# RDB

