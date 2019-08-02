# hash

# 目录

* [相关位置文件](#相关位置文件)
* [encoding](#encoding)
	* [OBJ_ENCODING_ZIPLIST](#OBJ_ENCODING_ZIPLIST)
		* [entry](#entry)
			* [prevlen](#prevlen)
			* [encoding](#encoding)
			* [entry data](#entry-data)
		* [增删改查](#增删改查)
            * [创建](#创建)
            * [读取](#读取)
            * [修改](#修改)
            * [删除](#删除)
		* [升级](#升级)
	* [OBJ_ENCODING_HT](#OBJ_ENCODING_HT)
		* [哈希碰撞](#哈希碰撞)
		* [resize](#resize)
		* [activerehashing](#activerehashing)

# 相关位置文件
* redis/src/ziplist.h
* redis/src/ziplist.c
* redis/src/dict.h
* redis/src/dict.c

# encoding

## OBJ_ENCODING_ZIPLIST

> ziplist 是一种特殊的双端链表, 它能最大程度地节省内存空间. 它可以存储字符串和整型值, 整型的存储方式是真正的整数, 而不是一串整数的字符串表示, 它也可以在任意一端进行压入/弹出操作, 并且该操作的时间复杂度为 O(1). 但是每一个操作都需要对这个 ziplist 的空间进行重新分配, 实际上真正的复杂度是和 ziplist 使用到的空间相关的

翻译自 `redis/src/ziplist.c`

这是 **ziplist** 的内存构造

![ziplist_overall_layout](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/ziplist_overall_layout.png)

我们来看一个简单的示例

    127.0.0.1:6379> HSET AA key1 33
    (integer) 1
    127.0.0.1:6379> OBJECT ENCODING AA
    "ziplist"

![simple_hash](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/simple_hash.png)

从上图我们可以看出

`zlbytes` 表示当前这个 **ziplist** 总共占用的字节数

`zltail` 是从当前位置到达 `zlend` 的字节偏移量

`zllen` 表示当前这个 **ziplist** 有多少个 entries

`zlend` 是一个长度为一字节的结束标记, 表示 **ziplist** 的尾端

这是上面插入过后的内存构造

![simple_hash_two_entry](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/simple_hash_two_entry.png)

`prevlen` 存储的是前一个 entry 占用的字节长度, 通过这个字段你可以反方向遍历这个双端链表, 这个字段本身的长度要么就是一个 1 字节长的 `uint8_t` 表示前一个 entry 的长度在 254 bytes 以下, 要么是一个 5 字节长的数, 第一个字节用来作为标记区分前一种表示方法, 后面的 4 个字节可以作为 `uint32_t` 表示前一个 entry 占用的字节数

`encoding` 表示的是 entry 中的内容是如何进行编码的, 当前存在好几种不同的 `encoding`, 我们后面会看到

### entry

我们现在知道了 entry 包含了 3 个部分

#### prevlen

`prevlen` 要么是 1 字节长, 要么是 5 字节长, 如果 `prevlen` 的第一个字节值为 254, 那么接下来的 4 个字节一起作为一个无符号整数表示前一个 entry 的长度

![prevlen](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/prevlen.png)

#### encoding

`entry data` 中内容的实际存储方式取决于 `encoding`, 总共有以下几种 `encoding`

![encoding](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/encoding.png)

#### entry data

`entry data` 存储了实际的内容, `entry data` 中的内容要么就是 byte 数组, 要么就是整型, 你需要根据 `encoding` 来获取 `entry data` 中的内容

### 增删改查

#### 创建

你第一次输入 `hset` 某个键时, 会首先创建一个空的 `ziplist`, 然后再把键对值插进去

![empty_ziplist](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/empty_ziplist.png)

    127.0.0.1:6379> HSET AA key1 val1
    (integer) 1
    127.0.0.1:6379> HSET AA key2 123
    (integer) 1

![two_value_ziplist](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/two_value_ziplist.png)

#### 读取

    127.0.0.1:6379> HGET AA key2
    "123"

`hget` 命令会遍历这个 `ziplist`, 根据 `encoding` 提取出 `entry data` 中的内容, 之后检查内容是否匹配, 它是一个 O(n) 的线性搜索

#### 修改

这是更新键值的代码片段

	/* redis/src/t_hash.c */
    /* Delete value */
    zl = ziplistDelete(zl, &vptr);

    /* Insert new value */
    zl = ziplistInsert(zl, vptr, (unsigned char*)value, sdslen(value));

我们来看一个示例

    127.0.0.1:6379> HSET AA key1 val_new
    (integer) 0

首先是删除旧的值

![delete_ziplist](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/delete_ziplist.png)

之后是插入新的值


插入的时候会调用 `realloc` 来扩展 `ziplist` 的空间, 并且把所有的待插入位置之后的 `entry` 后移, 然后插入到这个对应的位置上

`realloc` 会调用到 `redis/src/zmalloc.h` 中的 `zrealloc` 函数, 我们会在其他的文章学习到这部分内容

![update_ziplist](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/update_ziplist.png)

#### 删除

删除操作会把所有当前准备删除的 `entry` 后面的全部数据前移, 由于 `prevlen` 的大小要么是 1 字节 要么是 5 字节, 在以下情况有可能发生 `CascadeUpdate` 现象

比如当前删除的 `entry` 的前一个 `entry` 大小超过 254, 1 个字节无法存储, 但是当前的 `entry` 的长度又小于 254, 也就意味着下一个 `entry` 的 `prevlen` 字段是用 1 个字节存储的, 如果删除了当前的 `entry`, 下一个 `entry` 也需要做扩展, 对于再之后的 `entry` 也都要做相同的检查

假设我们正在删除下图的第二个 `entry`

![cascad](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/cascad.png)

在删除并把后面的全部数据前移之后

![cascad_promotion1](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/cascad_promotion1.png)

此时下一个 `entry` 的 `prevlen` 本身为 1 个字节, `ziplist` 需要调用 `realloc` 来扩展下一个 `entry` 的 `prevlen`

![cascad_promotion2](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/cascad_promotion2.png)

如果这个 `entry` 在扩展之后长度超过了 255 字节 ? 再下一个 `entry` 的 `prevlen` 此时又无法存下前一个 `entry` 的长度, 我们需要再次执行 `realloc`

![cascad_promotion3](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/cascad_promotion3.png)

如果你有一个拥有 N 个 `entry` 的 `ziplist`, 并且执行了删除操作, 那么每一个被删除的 `entry` 之后的 `entry` 都需要按顺序进行上面的检查, 看是否需要被扩展

这就是所谓的 **cascad update**

### 升级

	/* redis/src/t_hash.c */
    /* 在 hset 中执行 */
    for (i = start; i <= end; i++) {
        if (sdsEncodedObject(argv[i]) &&
            sdslen(argv[i]->ptr) > server.hash_max_ziplist_value)
        {
            hashTypeConvert(o, OBJ_ENCODING_HT);
            break;
        }
    }
	/* ... */
    /* 检查 ziplist 是否需要被转换为 hash table */
    if (hashTypeLength(o) > server.hash_max_ziplist_entries)
        hashTypeConvert(o, OBJ_ENCODING_HT);

如果插入的对象类型为 [sds](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/sds/sds_cn.md) 并且 sds 长度大于 `hash-max-ziplist-value` (你可以在配置文件中进行设置, 默认值为64), 当前的 `ziplist` 就会被转换为 hash table(`OBJ_ENCODING_HT`)

或者 `ziplist` 的长度在插入后超过了 `hash-max-ziplist-entries`(默认值为 512), 它也同样会被转换为 hash table

## OBJ_ENCODING_HT

我在配置文件配置了如下这行 `hash-max-ziplist-entries 0`

	hset AA key1 12

这是设置了一个键对值之后的构造

![dict_layout](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/dict_layout.png)

### 哈希碰撞

redis 目前使用 [SipHash 1-2](https://en.wikipedia.org/wiki/SipHash)(文件位置 `redis/src/siphash.c`) 作为哈希函数, 抛弃了在先前版本中使用的 [murmurhash](https://en.wikipedia.org/wiki/MurmurHash)

根据代码注释等信息, **SipHash 1-2** 的速度和 **Murmurhash2** 基本相同, 但是 **SipHash 2-4** 会大概拖慢 4% 左右的运行速度

并且哈希函数的 seed 是在 redis 服务启动时初始化的

	/* redis/src/server.c */
    char hashseed[16];
    getRandomHexChars(hashseed,sizeof(hashseed));
    dictSetHashFunctionSeed((uint8_t*)hashseed);

由于 **hashseed** 是随机生成的, 在不同的 redis 实例之间, 或者同个 redis 实例重启之后, 即使是相同的 key 哈希的结果也是不同的, 你无法预测这个 key 会被哈希到表的哪一个桶上

	hset AA zzz val2

CPython 使用了 [一个探针算法处理哈希碰撞](https://github.com/zpoint/CPython-Internals/blob/master/BasicObject/dict/dict_cn.md#%E5%93%88%E5%B8%8C%E7%A2%B0%E6%92%9E%E4%B8%8E%E5%88%A0%E9%99%A4), redis 使用的是单向链表

如果两个 key 的哈希取模后的值相同, 他们会被存储到同一个单向链表中, 并且新插入的对象会在前面

![dict_collision](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/dict_collision.png)

	hdel AA zzz

删除操作会找到对应的键对值删除, 并且在必要时调整哈希表的大小

![dict_delete](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/dict_delete.png)

### resize

在每一次字典插入时, 函数 `_dictExpandIfNeeded` 都会被调用

	/* redis/src/dict.c */
    /* 需要时扩展哈希表 */
    static int _dictExpandIfNeeded(dict *d)
    {
        /* 渐进式 rehash 正在进行中, 直接返回 */
        if (dictIsRehashing(d)) return DICT_OK;

        /* 如果哈希表为空, 把它初始化为默认大小. */
        if (d->ht[0].size == 0) return dictExpand(d, DICT_HT_INITIAL_SIZE);
        /* 如果达到了 1:1 的比例, 并且我们可以调整哈希表的大小,
           或者当前的负载比例已经超过了设定的安全范围, 我们就会把哈希表的大小调整为原先的 2 倍 */
        if (d->ht[0].used >= d->ht[0].size &&
            (dict_can_resize ||
             d->ht[0].used/d->ht[0].size > dict_force_resize_ratio))
        {
            return dictExpand(d, d->ht[0].used*2);
        }
        return DICT_OK;
    }

为了最大程序提高服务性能, 降低响应延时, **redis** 在字典中实现了 [渐进式 resizing](https://en.wikipedia.org/wiki/Hash_table#Incremental_resizing) 策略, 整个调整的过程并不是一次请求或者一次函数调用就完成的, 而是在每一次增删改查操作中一点一点完成的

我们来看个示例

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

![resize_before](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/resize_before.png)


    127.0.0.1:6379> hset AA 5 2
    (integer) 1

这一次我们插入新的 entry 时, `_dictExpandIfNeeded` 同样会被调用, 并且此时 `d->ht[0].used >= d->ht[0].size` 的判断为真, `dictExpand` 会新建一个 2 倍大小的哈希表, 并且把这张表存储到 `d->ht[1]` 中

	/* redis/src/dict.c */
    /* 扩展或创建哈希表 */
    int dictExpand(dict *d, unsigned long size)
    {
        / * 如果 size 比哈希表中存储的元素还要小, 那么这个 size 是非法的 */
        if (dictIsRehashing(d) || d->ht[0].used > size)
            return DICT_ERR;

        dictht n; /* 新的哈希表 */
        unsigned long realsize = _dictNextPower(size);

        /* Rehash 到的新表的大小不能和旧表大小一样 */
        if (realsize == d->ht[0].size) return DICT_ERR;

        /* 为新的哈希表分配空间, 并且把所有的指针初始化为空指针 */
        n.size = realsize;
        n.sizemask = realsize-1;
        n.table = zcalloc(realsize*sizeof(dictEntry*));
        n.used = 0;

        /* 如果是第一次调用这个函数分配表空间, 严格意义上这不是 rehash, 只用把表设置到 ht[0] 上即可 */
        if (d->ht[0].table == NULL) {
            d->ht[0] = n;
            return DICT_OK;
        }

        /* 把创建的表设置到 ht[1] 上这样可以进行渐进式 rehash */
        d->ht[1] = n;
        d->rehashidx = 0;
        return DICT_OK;
    }

因为 `rehashidx` 的值不是 -1, 新的 `entry` 插入到 `ht[1]` 中

![resize_middle](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/resize_middle.png)

当 `rehashidx` 的值不是 -1 时, 每一个增删改查操作都会调用一次 `rehash` 函数

    127.0.0.1:6379> hget AA 5
    "2"

你可以发 `rehashidx` 变成了 1, 并且在哈希表中的 index[1] 上的整个桶都被移到了第二张表中

![resize_middle2](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/resize_middle2.png)

    127.0.0.1:6379> hget AA not_exist
    (nil)

`rehashidx` 现在变成了 3

![resize_middle3](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/resize_middle3.png)

	127.0.0.1:6379> hget AA 5
	"2"

当哈希表中 index[3] 上的整个桶都迁移完成时, 这个哈希表也完整的迁移过去了, `rehashidx` 再次设置为 -1, 并且旧的表会被释放

![resize_done](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/resize_done.png)

并且新的表会变成 `ht[0]`, 两个表的位置会互相交换

![resize_done_reverse](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/resize_done_reverse.png)


	/* redis/src/dict.c */
    int dictRehash(dict *d, int n) {
        /* 每次执行 HGET 命令时, n 为 1 */
        int empty_visits = n*10; /* 最多只会处理这么多个哈希表上的空桶 */
        if (!dictIsRehashing(d)) return 0;

        while(n-- && d->ht[0].used != 0) {
            /* rehash n 个桶 */
            dictEntry *de, *nextde;
            /* 注意, rehashidx 不能溢出 */
            assert(d->ht[0].size > (unsigned long)d->rehashidx);
            while(d->ht[0].table[d->rehashidx] == NULL) {
                /* 这个桶是空的的话, 跳过它 */
                d->rehashidx++;
                if (--empty_visits == 0) return 1;
            }
            de = d->ht[0].table[d->rehashidx];
            /* 把这个桶上的所有的元素都移动到新的哈希表上 */
            while(de) {
                uint64_t h;

                nextde = de->next;
                /* 计算一下在新的表上的哈希值 */
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

        /* 检查是否 rehash 已经完成(是否整张表都迁移完成) */
        if (d->ht[0].used == 0) {
            zfree(d->ht[0].table);
            d->ht[0] = d->ht[1];
            _dictReset(&d->ht[1]);
            d->rehashidx = -1;
            return 0;
        }

        /* 本次处理完成, 但是还有待迁移的元素 */
        return 1;
    }

### activerehashing

如配置文件的注释所说

> Active rehashing 会在每 100 毫秒的间隔中(CPU 时间)拿出额外的 1 毫秒来处理 redis 服务的主哈希表的 rehash 操作(用来存储最顶层 db 的键对值的那张表) redis 中的哈希表使用的是 lazy rehashing 策略(参考 dict.c), 在一张正在 rehash 的表中, 你越频繁的操作这张表, rehash 就会越快的在这张表上转移旧元素到新表中, 所以如果你的 redis 服务处于闲置状态的话, 可能你的主表的 rehash 永远也不会完成, 一直处于 rehash 的状态之中, 会占用额外的内存空间

> 默认情况下在主表中每秒钟会处理 10 次持续 1 毫秒左右的 rehash, 并且在必要的时候释放空间
> 如果你不允许超过 2 毫秒的响应延时, 使用 "activerehashing no" 这个配置
> 如果你没有那么高的响应要求, 则用默认的 "activerehashing yes" 即可

redis 服务主表使用哈希表结构存储你设置的所有键对值, 如我们上面所了解的, 哈希表并不会在达到某个阈值之后一次性的扩容完成, 而是在你搜索/更改这张表的时候渐进式的完成扩容, `activerehashing` 这个配置会在主循环中用到, 用来处理空间的 redis 服务无法完成 rehash 的情况


	/* redis/src/server.c */
    /* Rehash */
    if (server.activerehashing) {
        for (j = 0; j < dbs_per_call; j++) {
            /* 每次调用这个函数的时候, 尝试使用 1 毫秒(CPU 时间) 来处理 rehah */
            int work_done = incrementallyRehash(rehash_db);
            if (work_done) {
                /* 如果当前的 rehash_db 处理了 1 毫秒的 rehash, 跳出, 下次主循环(100ms后)再来处理
                break;
            } else {
                /* 如果这个 rehash_db 不需要进行 rehash, 我们会尝试下一个 rehash_db */
                rehash_db++;
                rehash_db %= server.dbnum;
            }
        }
    }

