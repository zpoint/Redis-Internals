# list

# 目录

* [需要提前了解的知识](#需要提前了解的知识)
* [相关位置文件](#相关位置文件)
* [内存构造](#内存构造)
* [encoding](#encoding)
    * [OBJ_ENCODING_QUICKLIST](#OBJ_ENCODING_QUICKLIST)
    	* [quicklist](#quicklist)
    	* [quicklistNode](#quicklistNode)
    	* [示例](#示例)
    		* [list max ziplist size](#list-max-ziplist-size)
    		* [list compress depth](#list-max-ziplist-size)
    		* [插入](#插入)
    		* [删除](#删除)
* [更多资料](#更多资料)

# 需要提前了解的知识

* [redis 中 hash 对象的 ziplist 实现](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/hash_cn.md#OBJ_ENCODING_ZIPLIST)

# 相关位置文件
* redis/src/t_list.c
* redis/src/quicklist.h
* redis/src/quicklist.c
* redis/src/lzf.h
* redis/src/lzf_c.c
* redis/src/lzf_d.c
* redis/src/lzfP.h

# 内存构造

这是 `quicklist` 在 C 中的定义

![quicklist](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/list/quicklist.png)

# encoding

对于 `list` 对象, 早期的 redis 版本在数据量比较小时使用 `ziplist` 作为底层结构, 在数据量到临界值时转换为双端链表进行存储, **OBJ_ENCODING_ZIPLIST** 和 **OBJ_ENCODING_LINKEDLIST** 分别用来表示这两种结构, 他们升级转换的临界条件和 [hash 对象中的升级](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/hash_cn.md#%E5%8D%87%E7%BA%A7) 类似


当前的 Redis 版本不再使用的 `ziplist` 或者 双端链表, 而是引入了一种新的结构叫做 `quicklist`, 目的是最大程度减小内存空间占用并保证性能

> 总结: Quicklist 是 ziplists 的双端链表版本. Quicklist 结合了 ziplists 低内存占用和双端链表的高度扩展性(我们可以创建任意长度的列表)的优点

翻译自 [redis quicklist visions](https://matt.sh/redis-quicklist-visions)

## OBJ_ENCODING_QUICKLIST

    127.0.0.1:6379> lpush lst 123 456
    (integer) 2
    127.0.0.1:6379> object encoding lst
    "quicklist"

![example](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/list/example.png)

从上图我们可以发现, **quicklist** 存储了一个双端链表, 链表中的每一个节点都是一个 **quicklistNode** 对象, **quicklistNode** 中的 `zl` 指向了一个 **ziplist**, **ziplist** 在 [redis 中 hash 对象的 ziplist 实现](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/hash_cn.md#OBJ_ENCODING_ZIPLIST) 中有介绍到

### quicklist

`head` 和 `tail` 分别指向这个双端链表的表头和表尾, **quicklist** 存储的节点是一个叫做 **quicklistNode** 的结构, 如果这个 **quicklist** 是空的, 那么 `head` 和 `tail` 会同时成为空指针, 如果这个双端链表的大小为 1, 那么 `head` 和 `tail` 会同时指向一个相同的节点

`count` 是一个计数器, 表示当前这个 `list` 结构一共存储了多少个元素, 它的类型是 `unsigned long`, 所以一个 `list` 能存储的最多的元素在 字长为 `64 bit` 的机器上是 `(1 << 64) - 1`, 字长为 `32 bit` 的机器上是 `(1 << 32) - 1`

`len` 表示了这个双端链表的长度(quicklistNodes 的数量)

`fill` 表示了单个节点(quicklistNode)的负载比例(fill factor), 这是什么意思呢 ?

> Lists 结构使用了一种特殊的编码方式来节省空间, Lists 中每一个节点所能存储的东西可以通过最大长度或者一个最大存储的空间大小来限制, 对于想限制每个节点最大存储空间的用户, 用 -5 到 -1 来表示这个限制值
>
> -5: 最大存储空间: 64 Kb  <-- 通常情况下不要设置这个值
>
> -4: 最大存储空间: 32 Kb  <-- 非常不推荐
>
> -3: 最大存储空间: 16 Kb  <-- 不推荐
>
> -2: 最大存储空间: 8 Kb   <-- 推荐
>
> -1: 最大存储空间: 4 Kb   <-- 推荐
>
> 对于正整数则表示最多能存储到你设置的那个值, 当前的节点就装满了
>
> 通常在 -2 (8 Kb size) 或 -1 (4 Kb size) 时, 性能表现最好
>
> 但是如果你的使用场景非常独特的话, 调整到适合你的场景的值

翻译自 `redis.conf`, 其中有一个可配置的参数叫做 `list-max-ziplist-size`, 默认值为 -2, 它控制了 **quicklist** 中的 `fill` 字段的值, 负数限制 **quicklistNode** 中的 **ziplist** 的字节长度, 正数限制 **quicklistNode** 中的 **ziplist** 的最大长度

`compress` 则表示 **quicklist** 中的节点 **quicklistNode**, 除开最两端的 `compress` 个节点之后, 中间的节点都会被压缩

> Lists 在某些情况下是会被压缩的, 压缩深度是表示除开 list 两侧的这么多个节点不会被压缩, 剩下的节点都会被尝试进行压缩, 头尾两个节点一定不会被进行压缩, 因为要保证 push/pop 操作的性能, 有以下的值可以设置

> 0: 关闭压缩功能
>
> 1: 深度 1 表示至少在 1 个节点以后才会开始尝试压缩, 方向为从头到尾或者从尾到头
>
>    所以: [head]->node->node->...->node->[tail]
>
>    [head], [tail] 永远都是不会被压缩的状态; 中间的节点则会被压缩
>
> 2: [head]->[next]->node->node->...->node->[prev]->[tail]
>
>    2 在这里的意思表示不会尝试压缩 head 或者 head->next 或者 tail->prev 或者 tail
>
>    但是会压缩这中间的所有节点
>
> 3: [head]->[next]->[next]->node->node->...->node->[prev]->[prev]->[tail]
>
> 和上面的示例类似

翻译自 `redis.conf`, 其中有一个可配置的参数叫做 `list-compress-depth`, 默认值为 0(表示不压缩).  redis 使用了 [LZF 算法](https://github.com/ning/compress/wiki/LZFFormat) 进行压缩, 这个算法压缩比并没有其他算法如 `Deflate` 等高, 但是优点是速度快, 我们后面会看到示例

### quicklistNode

`prev` 和 `next` 分别指向当前 **quicklistNode** 的前一个和后一个节点

`zl` 指向实际的 **ziplist**

`sz` 存储了当前这个 **ziplist** 的占用空间的大小, 单位是字节

`count` 表示当前有多少个元素存储在这个节点的 **ziplist** 中, 它是一个 16 bit 大小的字段, 所以一个 **quicklistNode** 最多也只能存储 65536 个元素

`encoding` 表示当前节点中的 **ziplist** 的编码方式, 1(RAW) 表示默认的方式存储, 2(LZF) 表示用 LZF 算法压缩后进行的存储

`container` 表示 **quicklistNode** 当前使用哪种数据结构进行存储的, 目前支持的也是默认的值为 2(ZIPLIST), 未来也许会引入更多其他的结构

`recompress` 是一个 1 bit 大小的布尔值, 它表示当前的 **quicklistNode** 是不是已经被解压出来作临时使用

`attempted_compress` 只在测试的时候使用

`extra` 是剩下多出来的 bit, 可以留作未来使用

### 示例

#### list max ziplist size

我在配置文件做了如下两行的更改

	list-max-ziplist-size 3
    list-compress-depth 0

在 redis 客户端中

    127.0.0.1:6379> del lst
    (integer) 1
    127.0.0.1:6379> lpush lst val1 123 456
    (integer) 3

![max_size_1](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/list/max_size_1.png)

`fill`(每个节点最多能存储多少元素) 值为 3, `len`(节点数量) is 1, `count`(一共存储了多少个元素) 为 3

如果我们再进行一次插入

    127.0.0.1:6379> lpush lst 789
    (integer) 4

因为第一个节点中的元素数量已经大于等于 `fill` 中的值了, 一个新的节点会被创建

`fill` 仍然为 3, `len` 变为 2, `count` 变为 4

![max_size_2](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/list/max_size_2.png)

#### list compress depth

这是针对 **quicklistNode** 的压缩函数

	/* redis/src/quicklist.c */
    REDIS_STATIC int __quicklistCompressNode(quicklistNode *node) {
    #ifdef REDIS_TEST
        node->attempted_compress = 1;
    #endif

        /* 如果被压缩的部分字节数太少, 则不进行压缩 */
        /* 这个值默认是 48 bytes  */
        if (node->sz < MIN_COMPRESS_BYTES)
            return 0;
        /* 为压缩后的结果分配空间, 新分配的空间是原本未压缩之前的长度 + lzf 结构头部的长度 */
        quicklistLZF *lzf = zmalloc(sizeof(*lzf) + node->sz);

        /* 如果压缩失败或者压缩并未减少一定比例的空间, 则取消压缩 */
        if (((lzf->sz = lzf_compress(node->zl, node->sz, lzf->compressed,
                                     node->sz)) == 0) ||
            lzf->sz + MIN_COMPRESS_IMPROVE >= node->sz) {
            /* 如果传入的值无法压缩, lzf_compress 拒绝压缩 */
            zfree(lzf);
            return 0;
        }
        /* 成功的进行压缩, 调整之前分配的内存块的大小, 让他适配新的压缩后的大小, 避免浪费空间 */
        lzf = zrealloc(lzf, sizeof(*lzf) + lzf->sz);
        /* 释放原本的 ziplist */
        free(node->zl);
        /* 把压缩完后的数组地址存储到 zl 中 */
        node->zl = (unsigned char *)lzf;
        /* 标记上这是一个需要进行压缩的 ziplist */
        node->encoding = QUICKLIST_NODE_ENCODING_LZF;
        /* 标记上这个 ziplist 当前已经被压缩了 */
        node->recompress = 0;
        return 1;
    }

我在配置文件做了如下两行的更改

	list-max-ziplist-size 4
    list-compress-depth 1

并且为了简短的进行演示更改了这两行源代码

	#define MIN_COMPRESS_BYTES 1
    #define MIN_COMPRESS_IMPROVE 0

在 redis 客户端中

    127.0.0.1:6379> lpush lst aa1 aa2 aa3 aa4 bb1 bb2 bb3 bb4
    (integer) 8

![compress1](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/list/compress1.png)

    127.0.0.1:6379> lpush lst cc1 cc2 cc3 cc4
    (integer) 12

因为 `list-compress-depth` 的值为 1, 两边的第一个 **quicklistNode** 不会被压缩, 第二, 三, 四个之后的在插入之后都会检查下是否需要进行压缩

注意, 第二个 **quicklistNode** 的 `encoding` 变为了 2, 表示使用了 [LZF](https://github.com/ning/compress/wiki/LZFFormat) 算法在当前的节点上进行压缩, 这个算法基本的思想是把重复的部分用标记等方式来表示, 从而节省内存空间

![compress2](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/list/compress2.png)

#### 插入

如果你插入一个新的元素到 **quicklist** 中
* 如果当前的 **quicklist** 是空的, 创建一个新的 **ziplist** 并且把这个元素插入到 **ziplist** 中
* 如果当前的 **quicklist** 不是空的, 找到你想要插入的位置
* 如果这个要插入的节点还有位置(`count` < `fill`), 插入到当前节点的 **ziplist** 中
* 如果没位置了, 但是你插入的是 **quicklist** 的头部或者尾部(lpush/rpush), 创建一个新的 **ziplist** 并进行插入
* 不然的话(没位置并且插入中间), 把中间的 **ziplist** 按照你插入的位置为界, 分成两个 **ziplist**, 插入并连接起来

我们来插入到中间部分试试看

	list-max-ziplist-size 3
    list-compress-depth 0

在 redis 客户端中

    127.0.0.1:6379> del lst
    (integer) 1
    127.0.0.1:6379> lpush lst aa1 aa2 aa3 bb1 bb2 bb3 cc1 cc2 cc3
    (integer) 8

![insert_middle](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/list/insert_middle.png)

    127.0.0.1:6379> linsert lst after bb2 123
    (integer) 10

因为存储了值 **bb2** 的 **quicklistNode** 是满的了, 这个节点会被拆开

![insert_middle2](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/list/insert_middle2.png)

这里 `list` 的实现方式和 `C++` 中 `STL` 的 `dequeue` 有点类似

#### 删除

在 `redis/src/quicklist.c` 中定义了如下函数原型 `int quicklistDelRange(quicklist *quicklist, const long start, const long count)`, 这个函数会遍历所有的节点, 知道你指定的范围都删除为止

# 更多资料
* [redis quicklist](https://matt.sh/redis-quicklist)
* [redis quicklist visions](https://matt.sh/redis-quicklist-visions)
* [Redis内部数据结构详解(5)——quicklist](http://zhangtielei.com/posts/blog-redis-quicklist.html)
* [LZF format specification / description](https://github.com/ning/compress/wiki/LZFFormat)
