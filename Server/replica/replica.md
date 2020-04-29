# Replica

# contents

* [related file](#related-file)
* [REPLICAOF](#REPLICAOF)
* [read more](#read-more)

# related file

* redis/src/replication.c

## REPLICAOF

The `REPLICAOF` command is used for setting the current instance to be the follower of a specific redis instance, they act as master slave pattern

```shell script
127.0.0.1:6380> REPLICAOF localhost 6379
OK
```





# read more

[redis document -> replication](https://redis.io/topics/replication)

