# set

# 目录

* [需要提前了解的知识](#需要提前了解的知识)
* [相关位置文件](#相关位置文件)
* [encoding](#encoding)
	* [OBJ_ENCODING_INTSET](#OBJ_ENCODING_INTSET)
		* [INTSET_ENC_INT16](#INTSET_ENC_INT16)
		* [INTSET_ENC_INT32](#INTSET_ENC_INT32)
		* [INTSET_ENC_INT64](#INTSET_ENC_INT64)
	* [OBJ_ENCODING_HT](#OBJ_ENCODING_HT)
* [sdiff](#sdiff)
	* [算法 1](#算法-1)
	* [算法 2](#算法-2)


# 需要提前了解的知识

* [redis hash 结构中使用的 hashtable](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/hash_cn.md#OBJ_ENCODING_HT)

# 相关位置文件
* redis/src/intset.c
* redis/src/intset.h
* redis/src/t_set.c


# encoding

## OBJ_ENCODING_INTSET

这是 **intset** 的内存构造

![intset_layout](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/set/intset_layout.png)

### INTSET_ENC_INT16

对于 **intset** 来说总共有三种不同的 encoding, 它表示了 **intset** 中存储的实际内容是如何编码的

    127.0.0.1:6379> sadd set1 100
    (integer) 1

![sadd1](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/set/sadd1.png)

如果存储的值都是整型, 并且大小都不超过 `int16_t` 这个类型能表示的最大值, 那么这些值就会用 `int16_t` 这个类型来存储, 并且内部是一个数组结构

    127.0.0.1:6379> sadd set1 -3
    (integer) 1

![sadd2](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/set/sadd2.png)

我们可以发现这个数组结构内的数值是以升序的顺序排好序的

    127.0.0.1:6379> sadd set1 5
    (integer) 1

![sadd3](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/set/sadd3.png)

    127.0.0.1:6379> sadd set1 1
    (integer) 1

这是 **intset** 的插入部分的函数


	/* redis/src/intset.c */
    /* 对于指定 intset 插入一个整型 */
    intset *intsetAdd(intset *is, int64_t value, uint8_t *success) {
        uint8_t valenc = _intsetValueEncoding(value);
        uint32_t pos;
        if (success) *success = 1;
        /* 在必要的时候升级 encoding, 如果我们需要进行升级, 我们知道这个值要么插在最后 (>0),
         * 要么插在最前(<0), 因为已经有的类型无法表示这个值, 才需要进行升级 */
        if (valenc > intrev32ifbe(is->encoding)) {
            /* 这一步一定会成功, 所以我们不用更改 *success. 的值 */
            return intsetUpgradeAndAdd(is,value);
        } else {
            /* 如果这个值已经在当前的集合中, 就不需要再进行处理了
             * 在搜索不到这个值的时候, 这个函数会把 "pos" 设置为可以进行插入的位置 */
            if (intsetSearch(is,value,&pos)) {
                /* 进行二分查找 */
                if (success) *success = 0;
                return is;
            }
            /* 扩展 intset 的内存 */
            is = intsetResize(is,intrev32ifbe(is->length)+1);
            /* 把所有 pos 开始的到结束字节移动到以 pos+1 开始的位置 */
            if (pos < intrev32ifbe(is->length)) intsetMoveTail(is,pos,pos+1);
        }
        /* 把 pos 这个位置设置为指定的值 */
        _intsetSet(is,pos,value);
        /* 更改 length */
        is->length = intrev32ifbe(intrev32ifbe(is->length)+1);
        return is;
    }

首先, `intsetSearch`  会对排好序的数组进行二分查找, 如果这个值已经在里面了, 就直接返回, 不然的话重新申请 **intset** 的内存空间

![resize](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/set/resize.png)

`intsetMoveTail` 这个函数会把所有 `pos` 开始的到结束字节移动到以 `pos+1` 开始的位置

![move](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/set/move.png)

![after_move](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/set/after_move.png)

最后把这个值插入到对应的位置并把 `length` 的值设置为 `length+1`

![insert](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/set/insert.png)

### INTSET_ENC_INT32

这是 **intset** 中不同的 `encoding` 的定义

    #define INTSET_ENC_INT16 (sizeof(int16_t))
    #define INTSET_ENC_INT32 (sizeof(int32_t))
    #define INTSET_ENC_INT64 (sizeof(int64_t))

首先我们初始化一个 `set`

    127.0.0.1:6379> del set1
    (integer) 1
    127.0.0.1:6379> sadd set1 100 -1 5
    (integer) 3

![before_upgrade](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/set/before_upgrade.png)

如果你插入了一人无法用 `int16_t` 这个类型表示的值, 那么整个数组都会被升级为 `int32_t`

    127.0.0.1:6379> sadd set1 32768
    (integer) 1

![after_upgrade](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/set/after_upgrade.png)

    127.0.0.1:6379> srem set1 32768
    (integer) 1

对于 **intset** 只存在升级, 不存在降级

![after_upgrade_remove](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/set/after_upgrade_remove.png)

### INTSET_ENC_INT64

    127.0.0.1:6379> sadd set1 2147483648
    (integer) 1

![after_upgrade_64](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/set/after_upgrade_64.png)

## OBJ_ENCODING_HT

`OBJ_ENCODING_HT` 这个类型表示在 [hash 对象](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/hash_cn.md#OBJ_ENCODING_HT) 中进行过介绍了, 你可以前面的链接获得一个更直观的介绍

这是 `set` 对象的 sadd 部分的函数

	/* redis/src/t_set.c */
    else if (subject->encoding == OBJ_ENCODING_INTSET) {
        if (isSdsRepresentableAsLongLong(value,&llval) == C_OK) {
            uint8_t success = 0;
            subject->ptr = intsetAdd(subject->ptr,llval,&success);
            if (success) {
                /* 如果当前的 intset 含有太多的元素, 把它转换为一个通用的 set(哈希表存储) */
                if (intsetLen(subject->ptr) > server.set_max_intset_entries)
                    setTypeConvert(subject,OBJ_ENCODING_HT);
                return 1;
            }
        } else {
            /* 这个值无法用整型表示, 把它转换为一个通用的 set(哈希表存储) */
            setTypeConvert(subject,OBJ_ENCODING_HT);

            /* 这个 set 之前是一个 intset, 但是当前插入的值无法用整型表示
             * 所以用 dictAdd 来处理这个值 */
            serverAssert(dictAdd(subject->ptr,sdsdup(value),NULL) == DICT_OK);
            return 1;
        }
    }

我们从上面的代码可以看出, 如果这个集合的 encoding 是 `OBJ_ENCODING_INTSET`, 并且新插入的值可以用 `long long` 这个类型来表示, 那么它会被前面介绍过的方式插入这个 **intset**, 如果这个值无法用 `long long` 表示(或者它根本不是一个整型), 或者当前的  **intset** 的长度已经超过了 `set_max_intset_entries`(你可以在配置文件里位置这个值, 默认值是 512), 当前的 **intset** 会被转换成一个 [hash 表](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/hash_cn.md#OBJ_ENCODING_HT)

为了更方便的进行解释说明, 我在我的配置文件里更改了如下这行

	set-max-intset-entries 3

并且初始化了一个 `set`

    127.0.0.1:6379> del set1
    (integer) 1
    127.0.0.1:6379> sadd set1 100 -1 5
    (integer) 3

![before_upgrade](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/set/before_upgrade.png)

    127.0.0.1:6379> sadd set1 3
    (integer) 1

因为 **intset** 的长度超过了 `set-max-intset-entries`, 它 会被转换成一张 [hash 表](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/hash.md#OBJ_ENCODING_HT)

![hash](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/set/hash.png)

    127.0.0.1:6379> del set1
    (integer) 1
    127.0.0.1:6379> sadd set1 100
    (integer) 1

![before_converted](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/set/before_converted.png)

或者你插入了一个无法用整型表示的值

    127.0.0.1:6379> sadd set1 abc
    (integer) 1

![after_converted](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/set/after_converted.png)

# sdiff

对于 `sdiff` 命令, 实现了两种不同的算法, 函数名称为 `sunionDiffGenericCommand`, 文件位置在 `redis/src/t_set.c`

> 判定使用哪一个 DIFF 函数
>
> 算法 1 复杂度为 O(N*M), N 表示第一个 set 的元素数量, M 是一共有多少个集合
>
> 算法 2 复杂度为 O(N), N 表示所有集合里加起来一共有多少个元素
>
> 我们对于当前输入的集合, 先计算一遍哪一个算法更优

    if (op == SET_OP_DIFF && sets[0]) {
        long long algo_one_work = 0, algo_two_work = 0;

        for (j = 0; j < setnum; j++) {
            if (sets[j] == NULL) continue;

            algo_one_work += setTypeSize(sets[0]);
            algo_two_work += setTypeSize(sets[j]);
        }
        /* 通常情况下算法 1 有更优的常数时间复杂度, 并且做更少的比较,
         * 所以同样情况下给算法 1 更高的优先级 */
        algo_one_work /= 2;
        diff_algo = (algo_one_work <= algo_two_work) ? 1 : 2;

## 算法 1

> DIFF 算法 1:
>
> 我们通过如下方式来达到 sdiff 的目的, 遍历所有第一个集合里的元素, 并且只有在这个元素不在其他所有剩余集合里面时, 才把这个元素插入结果的集合的
>
> 通过这种方式, 我们的时间复杂度最差的情况下为 N\*M, N 为第一个集合的元素数量, M 是一共有多少个集合

* 把 set1 到 setN 以降序的方式进行排序
* 对于 set0 中的每个元素
* 检查这个元素是否包含在 set1 到 setN 中, 如果都不在的话, 把这个元素插入到结果的集合的

## 算法 2

> DIFF 算法 2:
>
> 把第一个集合中的所有的元素都添加到结果的集合中, 对于剩下的集合中的每一个元素, 都从这个集合移除掉这个元素
>
> 这是一个 O(N) 复杂度的算法, N 表示所有不同的集合的元素的个数的总和

* 对于 set0 的每一个元素
* 把这个元素插入到结果的集合的
* 对于 set1 到 setN 的每一个集合
* 对于这个集合中的每一个元素
* 从结果的集合中移除这个元素

