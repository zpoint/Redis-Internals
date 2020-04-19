# set

# contents

* [prerequisites](#prerequisites)
* [related file](#related-file)
* [encoding](#encoding)
	* [OBJ_ENCODING_INTSET](#OBJ_ENCODING_INTSET)
		* [INTSET_ENC_INT16](#INTSET_ENC_INT16)
		* [INTSET_ENC_INT32](#INTSET_ENC_INT32)
		* [INTSET_ENC_INT64](#INTSET_ENC_INT64)
	* [OBJ_ENCODING_HT](#OBJ_ENCODING_HT)
* [sdiff](#sdiff)
	* [Algorithm 1](#Algorithm-1)
	* [Algorithm 2](#Algorithm-2)

# prerequisites

* [hashtable in redis hash](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/hash.md#OBJ_ENCODING_HT)

# related file
* redis/src/intset.c
* redis/src/intset.h
* redis/src/t_set.c


# encoding

## OBJ_ENCODING_INTSET

This is the layout of **intset**

![intset_layout](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/set/intset_layout.png)

### INTSET_ENC_INT16

There're totally three different encodings for **intset**, it means how contents stored inside the **intset** are encoded

```shell script
127.0.0.1:6379> sadd set1 100
(integer) 1

```

![sadd1](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/set/sadd1.png)

If the value is integer and fits into `int16_t`, the value will be represent as type `int16_t` and stored inside an array like object

```shell script
127.0.0.1:6379> sadd set1 -3
(integer) 1

```

![sadd2](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/set/sadd2.png)

We can find that the value inside the array is sorted in ascending order

```shell script
127.0.0.1:6379> sadd set1 5
(integer) 1

```

![sadd3](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/set/sadd3.png)

```shell script
127.0.0.1:6379> sadd set1 1
(integer) 1

```

This is the insert function of **intset**

```c

/* redis/src/intset.c */
/* Insert an integer in the intset */
intset *intsetAdd(intset *is, int64_t value, uint8_t *success) {
    uint8_t valenc = _intsetValueEncoding(value);
    uint32_t pos;
    if (success) *success = 1;

    /* Upgrade encoding if necessary. If we need to upgrade, we know that
     * this value should be either appended (if > 0) or prepended (if < 0),
     * because it lies outside the range of existing values. */
    if (valenc > intrev32ifbe(is->encoding)) {
        /* This always succeeds, so we don't need to curry *success. */
        return intsetUpgradeAndAdd(is,value);
    } else {
        /* Abort if the value is already present in the set.
         * This call will populate "pos" with the right position to insert
         * the value when it cannot be found. */
        if (intsetSearch(is,value,&pos)) {
            /* perform binary search */
            if (success) *success = 0;
            return is;
        }
        /* extend the mermory of intset */
        is = intsetResize(is,intrev32ifbe(is->length)+1);
        /* move every bytes after pos to pos+1 */
        if (pos < intrev32ifbe(is->length)) intsetMoveTail(is,pos,pos+1);
    }
    /* set the value in pos */
    _intsetSet(is,pos,value);
    /* change the length */
    is->length = intrev32ifbe(intrev32ifbe(is->length)+1);
    return is;
}

```

First `intsetSearch` will do binary search in the sorted **intset**, if the value is founded, return the intset directly, else reallocate the **intset**

![resize](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/set/resize.png)

The `intsetMoveTail` will move every bytes after `pos` to `pos+1`

![move](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/set/move.png)

![after_move](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/set/after_move.png)

Finally inset the value into the right position and set the `length` to `length+1`

![insert](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/set/insert.png)

### INTSET_ENC_INT32

This is how `encoding` in **intset** defined

```c
#define INTSET_ENC_INT16 (sizeof(int16_t))
#define INTSET_ENC_INT32 (sizeof(int32_t))
#define INTSET_ENC_INT64 (sizeof(int64_t))

```

First initialize a `set`

```shell script
127.0.0.1:6379> del set1
(integer) 1
127.0.0.1:6379> sadd set1 100 -1 5
(integer) 3

```

![before_upgrade](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/set/before_upgrade.png)

If you insert a value that can not fit into `int16_t`, the whole array will be upgraded to `int32_t`

```shell script
127.0.0.1:6379> sadd set1 32768
(integer) 1

```

![after_upgrade](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/set/after_upgrade.png)

```shell script
127.0.0.1:6379> srem set1 32768
(integer) 1

```

There's only upgrade strategy in **intset**, no downgrade strategy

![after_upgrade_remove](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/set/after_upgrade_remove.png)

### INTSET_ENC_INT64

```shell script
127.0.0.1:6379> sadd set1 2147483648
(integer) 1

```

![after_upgrade_64](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/set/after_upgrade_64.png)

## OBJ_ENCODING_HT

The encoding `OBJ_ENCODING_HT` is introduced in [hash object](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/hash.md#OBJ_ENCODING_HT), you can refer to that article to have a better overview

This is part of the set add function

```c
/* redis/src/t_set.c */
else if (subject->encoding == OBJ_ENCODING_INTSET) {
    if (isSdsRepresentableAsLongLong(value,&llval) == C_OK) {
        uint8_t success = 0;
        subject->ptr = intsetAdd(subject->ptr,llval,&success);
        if (success) {
            /* Convert to regular set when the intset contains
             * too many entries. */
            if (intsetLen(subject->ptr) > server.set_max_intset_entries)
                setTypeConvert(subject,OBJ_ENCODING_HT);
            return 1;
        }
    } else {
        /* Failed to get integer from object, convert to regular set. */
        setTypeConvert(subject,OBJ_ENCODING_HT);

        /* The set *was* an intset and this value is not integer
         * encodable, so dictAdd should always work. */
        serverAssert(dictAdd(subject->ptr,sdsdup(value),NULL) == DICT_OK);
        return 1;
    }
}

```

We can learn from the above code that if the set's encoding is `OBJ_ENCODING_INTSET`, and the newly inserted value can fit into type `long long`, it will be inserted into the **intset**, if the value can't fit into type `long long` or the length of the **intset** is longer than `set_max_intset_entries`(configurable in config file, default value is 512), it will be coverted to [hash table](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/hash.md#OBJ_ENCODING_HT)

I've set this line in my configure file for illustration

```c
set-max-intset-entries 3

```

And initlalize a `set`

```shell script
127.0.0.1:6379> del set1
(integer) 1
127.0.0.1:6379> sadd set1 100 -1 5
(integer) 3

```

![before_upgrade](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/set/before_upgrade.png)

```shell script
127.0.0.1:6379> sadd set1 3
(integer) 1

```

Because the length of the **intset** is longer than `set-max-intset-entries`, it will be coverted to [hash table](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/hash/hash.md#OBJ_ENCODING_HT)

![hash](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/set/hash.png)

```shell script
127.0.0.1:6379> del set1
(integer) 1
127.0.0.1:6379> sadd set1 100
(integer) 1

```

![before_converted](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/set/before_converted.png)

Or if you insert a value that can't be represent as an integer

```shell script
127.0.0.1:6379> sadd set1 abc
(integer) 1

```

![after_converted](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/set/after_converted.png)

# sdiff

There are two algorithms for `sdiff` comand, The function name is `sunionDiffGenericCommand` and defined in `redis/src/t_set.c`

> Select what DIFF algorithm to use.
>
> Algorithm 1 is O(N*M) where N is the size of the element first set and M the total number of sets.
>
> Algorithm 2 is O(N) where N is the total number of elements in all the sets.
>
> We compute what is the best bet with the current input here.

```c
if (op == SET_OP_DIFF && sets[0]) {
    long long algo_one_work = 0, algo_two_work = 0;

    for (j = 0; j < setnum; j++) {
        if (sets[j] == NULL) continue;

        algo_one_work += setTypeSize(sets[0]);
        algo_two_work += setTypeSize(sets[j]);
    }

    /* Algorithm 1 has better constant times and performs less operations
     * if there are elements in common. Give it some advantage. */
    algo_one_work /= 2;
    diff_algo = (algo_one_work <= algo_two_work) ? 1 : 2;

```

## Algorithm 1

> DIFF Algorithm 1:
>
> We perform the diff by iterating all the elements of the first set, and only adding it to the target set if the element does not exist into all the other sets.
>
> This way we perform at max N*M operations, where N is the size of the first set, and M the number of sets.

* sort set1 to setN in descending order
* for every elemnet in set0
* check if the element is in set1 to setN, if not, insert it to output set

## Algorithm 2

> DIFF Algorithm 2:
>
> Add all the elements of the first set to the auxiliary set. Then remove all the elements of all the next sets from it.
>
> This is O(N) where N is the sum of all the elements in every set.

* for every element in set0
* add it to output set
* for every set in set1 to setN
* for every element in this set
* remove the element from the output set

