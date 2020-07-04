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

For every command 

```c
void call(client *c, int flags) {
  /* ... */
  /* Call propagate() only if at least one of AOF / replication
     * propagation is needed. Note that modules commands handle replication
     * in an explicit way, so we never replicate them automatically. */
    if (propagate_flags != PROPAGATE_NONE && !(c->cmd->flags & CMD_MODULE))
        propagate(c->cmd,c->db->id,c->argv,c->argc,propagate_flags);
  /* ... */
}
```

And `propagate` will call `replicationFeedSlaves` if needed

```c
void propagate(struct redisCommand *cmd, int dbid, robj **argv, int argc,
               int flags)
{
    if (server.aof_state != AOF_OFF && flags & PROPAGATE_AOF)
        feedAppendOnlyFile(cmd,dbid,argv,argc);
    if (flags & PROPAGATE_REPL)
        replicationFeedSlaves(server.slaves,dbid,argv,argc);
}
```



`serverCron` will call `replicationCron` every seconds

```c
/* src/server.c */
/* Replication cron function -- used to reconnect to master,
 * detect transfer failures, start background RDB transfers and so forth. */
run_with_period(1000) replicationCron();
```



# read more

[redis document -> replication](https://redis.io/topics/replication)

