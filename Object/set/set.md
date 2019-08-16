# set

# contents

* [prerequisites](#prerequisites)
* [related file](#related-file)
* [encoding](#encoding)

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

    127.0.0.1:6379> sadd set1 100
    (integer) 1

![sadd1](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/set/sadd1.png)

If the value is integer and fits into `int16_t`, the value will be represent as type `int16_t` and stored inside an array like object

    127.0.0.1:6379> sadd set1 300
    (integer) 1

