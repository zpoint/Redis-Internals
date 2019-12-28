# persistence

# contents

* [related file](#related-file)
* [AOF](#AOF)
	* [rewrite](#rewrite)
	* [when will it be triggered](#when will it be triggered)
	* [policy](#policy)
		* [everysec](#everysec)
		* [always](#always)
		* [no](#no)
* [RDB](#RDB)

# related file

* redis/src/aof.c
* redis/src/rdb.c
* redis/src/rdb.h
* redis/src/adlist.h
* redis/src/adlist.c

There're currently two types of persistence available in redis

# AOF

If we set `appendonly yes` in `redis.conf`

And in command line

    127.0.0.1:6379> set key1 val1
    OK

And we inspect the `appendonly` file

    cat appendonly.aof
    *2
    $6
    SELECT
    $1
    0
    *3
    $3
    set
    $4
    key1
    $4
    val1

We can find that the `aof` file stores commands one by one

symbol `*` follows the argument count

symbol `$` follows the next string length

Let's read my `appendonly.aof` as example

`*2` means the following command has 2 arguments

The first argument is `$6\r\n` follow by `SELECT\r\n`, it means the string length is 6, if you read the next 6 characters, `SELECT` is the target string with exact length 6

The second argument is `$1\r\n` follow by `0\r\n`, it means the string lengtg is 1, if you read the next 1 character, you can get `0`

Together, it's `SELECT 0`

Similarly, the next command is `set key1 val1`

## rewrite



## when will it be triggered

redis server will call `propagate` for every command, `propagate` will call `feedAppendOnlyFile`, `feedAppendOnlyFile` will do some pre process(translate EXPIRE/PEXPIRE/EXPIREAT into PEXPIREAT) and save the command as the aformentioned format to `server.aof_buf`

    if (propagate_flags != PROPAGATE_NONE && !(c->cmd->flags & CMD_MODULE))
        propagate(c->cmd,c->db->id,c->argv,c->argc,propagate_flags);

    void propagate(struct redisCommand *cmd, int dbid, robj **argv, int argc, int flags)
    {
        if (server.aof_state != AOF_OFF && flags & PROPAGATE_AOF)
            feedAppendOnlyFile(cmd,dbid,argv,argc);
        if (flags & PROPAGATE_REPL)
            replicationFeedSlaves(server.slaves,dbid,argv,argc);
    }

    void feedAppendOnlyFile(struct redisCommand *cmd, int dictid, robj **argv, int argc) {
        /* ... */
        if (server.aof_state == AOF_ON)
            server.aof_buf = sdscatlen(server.aof_buf,buf,sdslen(buf));
        sdsfree(buf);
    }

`beforeSleep` will be called every time redis is entering the main loop of the event driven library, and before to sleep for ready file descriptors

    void beforeSleep(struct aeEventLoop *eventLoop) {
        /* ... */
        /* Write the AOF buffer on disk */
        flushAppendOnlyFile(0);
        /* ... */
    }


## policy

We can learn from the code that for every command, it will be pushed into the redis `server.aof_buf` buffer

    void flushAppendOnlyFile(int force) {
    	/* ... */

        if (server.aof_fsync == AOF_FSYNC_EVERYSEC)
            sync_in_progress = aofFsyncInProgress();

        if (server.aof_fsync == AOF_FSYNC_EVERYSEC && !force) {
        	/* ... */
            /* return depends on situation */
        }
		/* aofWrite calls the system call 'write' */
        latencyStartMonitor(latency);
        nwritten = aofWrite(server.aof_fd,server.aof_buf,sdslen(server.aof_buf));
        latencyEndMonitor(latency);

    try_fsync:
        /* Don't fsync if no-appendfsync-on-rewrite is set to yes and there are
         * children doing I/O in the background. */
        if (server.aof_no_fsync_on_rewrite &&
            (server.aof_child_pid != -1 || server.rdb_child_pid != -1))
                return;

        /* Perform the fsync if needed. */
        if (server.aof_fsync == AOF_FSYNC_ALWAYS) {
            /* redis_fsync is defined as fdatasync() for Linux in order to avoid
             * flushing metadata. */
            latencyStartMonitor(latency);
            redis_fsync(server.aof_fd); /* Let's try to get this data on the disk */
            latencyEndMonitor(latency);
            latencyAddSampleIfNeeded("aof-fsync-always",latency);
            server.aof_fsync_offset = server.aof_current_size;
            server.aof_last_fsync = server.unixtime;
        } else if ((server.aof_fsync == AOF_FSYNC_EVERYSEC &&
                    server.unixtime > server.aof_last_fsync)) {
            if (!sync_in_progress) {
                aof_background_fsync(server.aof_fd);
                server.aof_fsync_offset = server.aof_current_size;
            }
            server.aof_last_fsync = server.unixtime;
        }
    }


![aof_buffer](https://github.com/zpoint/Redis-Internals/blob/5.0/server/persistence/aof_buffer.png)

### everysec

### always

### no


# RDB

