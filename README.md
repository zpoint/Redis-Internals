# Redis Internals

* [简体中文](https://github.com/zpoint/Redis-Internals/blob/5.0/README_CN.md)
*  **Watch** this repo if you need to be notified when there's update

This repository is my notes for [redis](https://github.com/antirez/redis) source code

    # based on version 5.0.5
    cd redis
    git fetch origin 5.0:5.0
    git reset --hard 388efbf8b661ce2e5db447e994bf3c3caf6403c6

# Table of Contents

* [Objects](#Objects)
* [Server](#Server)
* [Why this repo](#Why-this-repo)
* [Learning material](#Learning-material)

# Objects
 - [x] [string(sds)](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/sds/sds.md)
 - [x] [hash(ziplist/ht)](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/hash.md)
 - [x] [list(quicklist)](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/list/list.md)
 - [x] [set(intset/ht)](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/set/set.md)
 - [x] [zset(ziplist/skiplist/ht)](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/zset/zset.md)
 - [x] [hyperloglog](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hyperloglog/hyperloglog.md)
 - [ ] stream

# Server
- persistence
- pubsub
- protocol
- transaction
- cluster
- redlock

# Why this repo

* learning purpose
* there are very good chinese [learning material](#learning-material), but no english version
* the book in [learning material](#learning-material) is based on redis 3.0, the implenentation detail may changed a lot from 3.0 to the current version

# Learning material

* [Redis 设计与实现](http://redisbook.com/)
* [Redis Documentation](https://redis.io/documentation)
* [Redis 5.0 RELEASENOTES](https://raw.githubusercontent.com/antirez/redis/5.0/00-RELEASENOTES)
