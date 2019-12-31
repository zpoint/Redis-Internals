# persistence

# contents

* [related file](#related-file)
* [AOF](#AOF)
	* [rewrite](#rewrite)
	* [when will aof be triggered](#when-will-aof-be-triggered)
	* [policy](#policy)
		* [everysec](#everysec)
		* [always](#always)
		* [no](#no)
* [RDB](#RDB)
	* [when will rdb be triggered](#when-will-rdb-be-triggered)

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

If we keep appending command to the tail of the `AOF` file, it's a matter of time that the `AOF` file will run out of the disk size, or it will become a very large file and takes a long time to reload or process the contents inside the `AOF` file

There's another strategy called `AOF rewrite` to prevent this from happening

	int serverCron(struct aeEventLoop *eventLoop, long long id, void *clientData) {
    	/* ... */
        /* Trigger an AOF rewrite if needed. */
        if (server.aof_state == AOF_ON &&
            server.rdb_child_pid == -1 &&
            server.aof_child_pid == -1 &&
            server.aof_rewrite_perc &&
            server.aof_current_size > server.aof_rewrite_min_size)
        {
            long long base = server.aof_rewrite_base_size ?
                server.aof_rewrite_base_size : 1;
            long long growth = (server.aof_current_size*100/base) - 100;
            if (growth >= server.aof_rewrite_perc) {
                serverLog(LL_NOTICE,"Starting automatic rewriting of AOF on %lld%% growth",growth);
                rewriteAppendOnlyFileBackground();
            }
        }

`aof_rewrite_perc` is configured in the configure file, it's the growth rate that will trigger the background `AOF rewrite` automatically

![aofrewrite](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/persistence/aofrewrite.png)

## when will aof be triggered

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


![aof_buffer](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/persistence/aof_buffer.png)

We can learn from the code that for every command, it will be pushed into the redis `server.aof_buf` buffer, and another routine will call the system call `write` to write the contents in `server.aof_buf` to operating system's buffer

And the different policy controls when system buffer will be flushed

### everysec

`fsync` will be called every second(not strictly), and the `fsync` is called by another thread, because commands with a second may block the master thread of redis, so redis is not single-thread server precisely

### always

`fsync` will be called every time in the main loop if there are contents in `server.aof_buf`, and it's directly called by the master thread

### no

`fsync` won't be called, the operating system takes full control of when the buffer in `server.aof_buf` will be flushed to disk

# RDB

![rdb](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/persistence/rdb.png)

## when will rdb be triggered

	int serverCron(struct aeEventLoop *eventLoop, long long id, void *clientData) {
        if (server.rdb_child_pid != -1 || server.aof_child_pid != -1 ||
            ldbPendingChildren())
        {
            /* ... */
        } else {
            /* If there is not a background saving/rewrite in progress check if
             * we have to save/rewrite now. */
            for (j = 0; j < server.saveparamslen; j++) {
                struct saveparam *sp = server.saveparams+j;

                /* Save if we reached the given amount of changes,
                 * the given amount of seconds, and if the latest bgsave was
                 * successful or if, in case of an error, at least
                 * CONFIG_BGSAVE_RETRY_DELAY seconds already elapsed. */
                if (server.dirty >= sp->changes &&
                    server.unixtime-server.lastsave > sp->seconds &&
                    (server.unixtime-server.lastbgsave_try >
                     CONFIG_BGSAVE_RETRY_DELAY ||
                     server.lastbgsave_status == C_OK))
                {
                    serverLog(LL_NOTICE,"%d changes in %d seconds. Saving...",
                        sp->changes, (int)sp->seconds);
                    rdbSaveInfo rsi, *rsiptr;
                    rsiptr = rdbPopulateSaveInfo(&rsi);
                    rdbSaveBackground(server.rdb_filename,rsiptr);
                    break;
                }
            }
        /* ... */
    }