# persistence

# 目录

* [相关位置文件](#相关位置文件)
* [AOF](#AOF)
	* [重写](#重写)
	* [aof什么时候会被触发](#aof什么时候会被触发)
	* [策略](#策略)
		* [everysec](#everysec)
		* [always](#always)
		* [no](#no)
* [RDB](#RDB)
	* [rdb什么时候会被触发](#rdb什么时候会被触发)
	* [策略](#策略)
* [更多资料](#更多资料)

# 相关位置文件

* redis/src/aof.c
* redis/src/rdb.c
* redis/src/rdb.h
* redis/src/adlist.h
* redis/src/adlist.c

redis中当前总共有两种不同的持久化策略

# AOF

AOF 是 Append Only File 的简称

如果我们在配置文件 `redis.conf` 中进行如下设置 `appendonly yes`

并且在redis客户端中

    127.0.0.1:6379> set key1 val1
    OK

此时我们查看 `appendonly` 文件可以发现如下内容

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

我们可以发现 `aof` 文件逐个的对各个命令进行存储

符号 `*` 后面跟的是命令参数数量

符号 `$` 后面跟的是字符串长度

我们以我的 `appendonly.aof` 文件为例

`*2` 表示接下来的命令有 2 个参数

第一个参数为 `$6\r\n`(紧跟着 `SELECT\r\n`), 表示字符串长度为 6, 并且你往下再读取 6 个字符就能获取到对应的命令, `SELECT`  刚好就是下一个字符串长度为 6 的命令

第二个参数为 `$1\r\n`(紧跟着 `0\r\n`), 表示字符串长度为 1, 如果你读取下一个字符, 读取到的参数为 `0`

拼起来这个命令为 `SELECT 0`

同理可得, 下一个命令为 `set key1 val1`

## 重写

如果我们持续的往 `AOF` 文件末尾追加命令内容, 假以时日, 这个 `AOF` 文件会把硬盘空间占满, 或者没有占满但是变得非常的大, 以至于redis会花费非常多的时间去读取和处理文件里的备份内容

为了防止这种情况发生, 引入了一种新的策略叫 `AOF 重写`

	int serverCron(struct aeEventLoop *eventLoop, long long id, void *clientData) {
    	/* ... */
        /* 在必要的时候触发 AOF 重写 */
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

`aof_rewrite_perc` 可以在配置文件中进行配置, 它是一个增长比例, 达到这个比例时就自动后台触发 `AOF` 重写

![aofrewrite](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/persistence/aofrewrite.png)

## aof什么时候会被触发

每一个命令在执行过程中会调用 `propagate` 函数, `propagate` 会调用 `feedAppendOnlyFile`, `feedAppendOnlyFile` 会做一些预处理(比如把 EXPIRE/PEXPIRE/EXPIREAT 命令转换为 PEXPIREAT 命令), 并且把预处理之后的命令以上述的格式存储到 `server.aof_buf` 缓冲区中

	void call(client *c, int flags)
    {
        /* ... */
        if (propagate_flags != PROPAGATE_NONE && !(c->cmd->flags & CMD_MODULE))
            propagate(c->cmd,c->db->id,c->argv,c->argc,propagate_flags);
    }

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


`beforeSleep` 会在每一次事件驱动循环中被调用, 调用之后才会等待下一个可用的文件描述符

    void beforeSleep(struct aeEventLoop *eventLoop) {
        /* ... */
        /* 把 AOF buffer 中的内容写到硬盘中 */
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
            /* 看情况决定是否返回 */
        }
		/* aofWrite 会进行 'write' 这个系统调用 */
        latencyStartMonitor(latency);
        nwritten = aofWrite(server.aof_fd,server.aof_buf,sdslen(server.aof_buf));
        latencyEndMonitor(latency);

    try_fsync:
        /* 如果 no-appendfsync-on-rewrite 设置为 yes 并且有子进程在后台进行 I/O 操作, 则调用 fsync 把系统缓冲区内容刷到硬盘 */
        if (server.aof_no_fsync_on_rewrite &&
            (server.aof_child_pid != -1 || server.rdb_child_pid != -1))
                return;

        /* 在必要的时候调用 fsync */
        if (server.aof_fsync == AOF_FSYNC_ALWAYS) {
            /* 在 Linux 平台下, redis_fsync 被定义为 fdatasync(), 目的是避免刷新 metadata */
            latencyStartMonitor(latency);
            redis_fsync(server.aof_fd); /* 把系统缓冲区的内容刷到磁盘 */
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

从代码中可以发现, 对于每一个命令, 它会以特定的格式被追加到 `server.aof_buf` 中, 并且另一个地方会调用系统函数 `write` 把 `server.aof_buf` 中的内容写到操作系统的缓冲区里

并且根据配置文件中不同的同步设置, 控制着什么时候对这个系统缓冲区的内容进行刷磁盘的操作

### everysec

`fsync` 会每间隔一秒就被调用一次(并不是严格的1秒的间隔), 并且 `fsync` 会在另一个线程中进行调用, 因为间隔 1 秒以上的执行的命令较多时, `fsync` 有可能会阻塞掉 redis 主线程, 所以严格意义上来讲, redis 并不是一个 单进程 - 单线程 的服务

### always

`fsync` 在每一次任务执行完后都会被调用, 并且是 master thread 直接进行的调用

### no

`fsync` 不会被主动调用, 由操作系统控制刷新到磁盘的时机, 通常是操作系统的缓冲区满了之后刷入磁盘

# RDB

RDB 是 Redis - DataBase 的简称

如果我们在配置文件 `redis.conf` 中把 `appendonly no` 功能关闭

然后再 redis 客户端中插入一个键对值

    127.0.0.1:6379> set key1 val1
    OK

我们开启 `redis-server` 并且查看下 rdb 文件


	% od -A d -t cu1 -c dump.rdb
    0000000    R   E   D   I   S   0   0   0   9 372  \t   r   e   d   i   s
               82  69  68  73  83  48  48  48  57 250   9 114 101 100 105 115
               R   E   D   I   S   0   0   0   9 372  \t   r   e   d   i   s
    0000016    -   v   e   r  \v   9   9   9   .   9   9   9   .   9   9   9
               45 118 101 114  11  57  57  57  46  57  57  57  46  57  57  57
               -   v   e   r  \v   9   9   9   .   9   9   9   .   9   9   9
    0000032  372  \n   r   e   d   i   s   -   b   i   t   s 300   @ 372 005
              250  10 114 101 100 105 115  45  98 105 116 115 192  64 250   5
             372  \n   r   e   d   i   s   -   b   i   t   s 300   @ 372 005
    0000048    c   t   i   m   e 302 300   D  \f   ^ 372  \b   u   s   e   d
               99 116 105 109 101 194 192  68  12  94 250   8 117 115 101 100
               c   t   i   m   e 302 300   D  \f   ^ 372  \b   u   s   e   d
    0000064    -   m   e   m 302 340   = 017  \0 372  \f   a   o   f   -   p
               45 109 101 109 194 224  61  15   0 250  12  97 111 102  45 112
               -   m   e   m 302 340   = 017  \0 372  \f   a   o   f   -   p
    0000080    r   e   a   m   b   l   e 300  \0 376  \0 373 001  \0  \0 004
              114 101  97 109  98 108 101 192   0 254   0 251   1   0   0   4
               r   e   a   m   b   l   e 300  \0 376  \0 373 001  \0  \0 004
    0000096    k   e   y   1 004   v   a   l   1 377  \n   $   @   q 306   ;
              107 101 121  49   4 118  97 108  49 255  10  36  64 113 198  59
               k   e   y   1 004   v   a   l   1 377  \n   $   @   q 306   ;
    0000112    Y 274
               89 229

上面的内容是一个只有一个 key 值的 rdb 文件, 最开始的内容为

`REDIS0009` 是一个开始标志字符串, `"REDIS%04d"` 最后面的4位数为版本号

`372` 是十进制数 `250` 的八进制表示, 它是一个标记, 表示下一个读取的值会是 RDB OPCODE(`#define RDB_OPCODE_AUX        250   /* RDB aux field. */`)

`9` 是下一个字符串长度(key), `redis-ver` 就是接下来的 9 个字符

`11` 是下一个字符串长度(value), `999.999.999` 就是这 11 个字符

接下来在 `0000032` 这行的 `250` 也是同一个标志, 表示接下来读取的还是一个辅助值, 读取出来是长度为 10 的 key `redis-bits`, 下一个字节是 `192`, 二进制值为 `11000000`, 表示下一个值是长度为 1 字节的有符号整数, 值为 `64`

整数值的编码方式为 标记 + 真正的整数值, 具体的整数值的长度和编码方式取决于这个整数值的长短

![rdb_enc_int](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/persistence/rdb_enc_int.png)

下一个 `250` 也是同样的辅助标记, 随后是长度为 5 的字符串 `ctime`, 随后的值为 `194`, 二进制为 `11000010`, 表示接下来的值是一个 4 字节的整型, 它是以小端存储的, 我文件里的值为 `0x5e0c044e(1577845838)`(一个时间戳)

下一个 `250` 还是同样的辅助标记, 随后是长度为 8 的字符串 `used-mem`, 随后的值为 `194`, 同样表示下一个值为 4 个字节的整型 `0x000f3de0(998880)`(使用的内存大小)

同理可得, 下一个辅助值的 key 为 `aof-preamble`, 值为 `0`

下一个字节为 `254`(`#define RDB_OPCODE_SELECTDB   254   /* DB number of the following keys. */`), 随后跟着一个整数 `0`, 表示当前的 db 序号为 `0`

下一个字节为 `251`(`#define RDB_OPCODE_RESIZEDB   251   /* Hash table resize hint. */`), 随后跟着一个整数 `1`, 表示当前的 db 的大小为 `1`

随后的 `0` 表示 `db->expires` 中的字典大小为 `0`

随后的 `0` 表示类型 `RDB_TYPE_STRING`

下一个值是长度为 4 的 key `key1` 和长度为 4 的值 `val1`

接下来的 `255` 表示 RDB 文件的结尾 `RDB_OPCODE_EOF`(`#define RDB_OPCODE_EOF        255   /* End of the RDB file. */`)

最后的 8 个字节为 checksum 大小(小端存储)


![rdb](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/persistence/rdb.png)

## rdb什么时候会被触发

	int serverCron(struct aeEventLoop *eventLoop, long long id, void *clientData) {
        if (server.rdb_child_pid != -1 || server.aof_child_pid != -1 ||
            ldbPendingChildren())
        {
            /* ... */
        } else {
            /* 如果当前的后台没有 saving/rewrite 操作, 检查我们是否需要进行 save/rewrite 操作 */
            for (j = 0; j < server.saveparamslen; j++) {
                struct saveparam *sp = server.saveparams+j;

                /* 如果达到了指定数量的修改, 或者指定的时间, 并且最近的 bgsave 是成功的, 或者最近的 bgsave 失败但是已经过了 CONFIG_BGSAVE_RETRY_DELAY 秒钟了 */
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


## 策略

下面的这一行配置表示在

> 900秒内(15分钟) 至少有 1 个 key 被修改了

则执行 `rdbSaveBackground`

	save 900 1

你可以指定多条规则, 所有的规则会被存储在 `server.saveparam` 这个数组中, 如果当前的服务状态匹配上了数组中的任意一个规则, `serverCron` 这个函数就会触发 `rdbSaveBackground`

# 更多资料
* [Redis 官方文档->持久化](https://redis.io/topics/persistence)
