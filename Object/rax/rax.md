# rax

# contents

* [related file](#related-file)
* [memory layout](#memory-layout)
* [internal](#internal)
* [read more](#read-more)



# related file
* redis/src/rax.h
* redis/src/rax.c

# memory layout

is a redis implementation [radix tree](https://en.wikipedia.org/wiki/Radix_tree), it's a a space-optimized prefix tree which is used in many places in redis, such as storing `streams` consumer group and cluster keys


# internal




# read more
[radix tree(wikipedia)](https://en.wikipedia.org/wiki/Radix_tree)
