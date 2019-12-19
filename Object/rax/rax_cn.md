# rax

# 目录

* [相关位置文件](#相关位置文件)
* [内存构造](#内存构造)
* [内部实现](#内部实现)
* [更多资料](#更多资料)



# 相关位置文件
* redis/src/rax.h
* redis/src/rax.c

# 内存构造

**rax** 是 redis 自己实现的[基数树](https://zh.wikipedia.org/wiki/%E5%9F%BA%E6%95%B0%E6%A0%91), 它是一种基于存储空间优化的前缀树数据结构, 在 redis 的许多地方都有使用到, 比如 `streams` 这个类型里面的 `consumer group`(消费者组) 的名称还有集群名称

![rax](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/rax/rax.png)

# 内部实现


    127.0.0.1:6379> xadd mystream * key1 val1
    "1575180011273-0"
    127.0.0.1:6379> XGROUP CREATE mystream mygroup1 $
    OK

通常来讲, 一个基数树(前缀树) 看起来如下所示

     * 这是最基本的表示方式:
     *
     *              (f) ""
     *                \
     *                (o) "f"
     *                  \
     *                  (o) "fo"
     *                    \
     *                  [t   b] "foo"
     *                  /     \
     *         "foot" (e)     (a) "foob"
     *                /         \
     *      "foote" (r)         (r) "fooba"
     *              /             \
     *    "footer" []             [] "foobar"
     *

翻译自 `redis/src/rax.h`

> 然而, 当前的代码实现使用了一种非常常见的优化策略, 把只有单个子的节点连续几个节点压缩成一个节点, 这个节点有一个字符串, 不再是只存储单个字符, 上述的结构可以优化成如下结构

     *
     *                  ["foo"] ""
     *                     |
     *                  [t   b] "foo"
     *                  /     \
     *        "foot" ("er")    ("ar") "foob"
     *                 /          \
     *       "footer" []          [] "foobar"

字符串 `mygroup1` 在 rax 中也是以压缩节点的方式存储的, 可以用如下表示

     *
     *                  ["mygroup1"] ""
     *                     |
     *                    [] "mygroup1"

第一个节点存储了压缩过的整个字符串 `mygroup1`, 第二个节点是一个空的叶子节点, 他是一个 key 值, 表示到这个节点之前合起来的字符串存储在了当前的 `rax` 结构中

下图是字符串 `mygroup1` 当前所在的 `rax` 的实际图示, 并且 key 节点中的 data 字段存储了一个指针, 这个指针指向了和 `mygroup1` 相对应的 `consumer group` 对象

`iskey` 表示当前的节点是否为 key 节点

`isnull` 表示当前节点是否有存储额外的值(比如上面的 `consumer group` 对象)

`iscompr` 表示当前节点是否为压缩的节点

`size` 是子节点数量或者压缩的字符串长度

![mygroup1](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/rax/mygroup1.png)

第一个节点的 `iscompr` 值为 1, 并且整个字符串 `mygroup1` 存储在了当前这一个节点中, `size` 为 8 表示当前节点存储了 8 个 `char` 字符, `iskey` 为 0, 表示当前的节点不是 key 节点, 我们需要继续往下搜索

第二个节点的 `iskey` 为 1, 表示当前的节点为 key 节点, 它表示在到这个节点之前的所有字符串连起来(也就是`mygroup1`) 存在当前的前缀树中, 也就是说当前的前缀树有包含 `mygroup1` 这个字符串, `isnull` 为 0 表示在当前这个 key 节点的 data 尾部存储了一个指针, 这个指针是函数调用者进行存储的, 在当前的情况它是一个指向 `streamCG` 的指针, 但是实际上他可以是指向任意对象的指针, 比如集群名称或者其他对象

我们再来插入一个 `consumer group` 名称到当前的前缀树中

    127.0.0.1:6379> XGROUP CREATE mystream mygroup2 $
    OK

![mygroup2](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/rax/mygroup2.png)

从上图可知, 第一个节点被裁剪了, 并且它后面插入了一个新的节点, 左下角的节点是原先存在的节点, 右下角的节点也是一个新插入的节点

     *
     *                  ["mygroup"] ""
     *                     |
     *                  [1   2] "mygroup"
     *                  /     \
     *      "mygroup1" []     [] "mygroup2"

中间的节点未被压缩(`iscompr` 这个位没有被设置), data 字段中存储了 `size` 个字符, 在这些字符之后, 同样会存储 `size` 个指向与之前字符一一对应的 `raxNode` 的结构的指针

底下两个节点的 `iskey = 1` 并 `isnull = 0`, 表示当到达任意一个这样的节点时, 当前的字符串是存储在这个前缀树中的, 并且在 data 字段最尾部存储了一个辅助的指针, 这个指针具体指向什么对象取决于调用者

如果你对 `rax` 的插入或者删除算法感兴趣, 请阅读以下代码

`redis/src/rax.c` 中的 `raxGenericInsert` 和 `redis/src/rax.c` 中的 `raxRemove`

# 更多资料

* [基数树(wikipedia)](https://zh.wikipedia.org/wiki/%E5%9F%BA%E6%95%B0%E6%A0%91)
* [Rax, an ANSI C radix tree implementation](https://github.com/antirez/rax)
