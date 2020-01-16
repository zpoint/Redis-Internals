# failover

# contents

* [prerequisites](#prerequisites)

# prerequisites

* [cluster](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/cluster.md)

The cluster example is set up in [prerequisites->cluster](https://github.com/zpoint/Redis-Internals/blob/5.0/Server/cluster/cluster.md)

If we add a new node `127.0.0.1:7003`

    127.0.0.1:7003> CLUSTER MEET 127.0.0.1 7000
    OK
    127.0.0.1:7003> CLUSTER NODES
    fd1099f4aff0be7fb014e1473c404631a764ffa4 127.0.0.1:7001@17001 master - 0 1579016073598 2 connected 5461-9188 9190-10922
    4e1901ce95cfb749b94c435e1f1c123ae0579e79 127.0.0.1:7000@17000 master - 0 1579016073000 4 connected 0-5460 9189
    a9b6ad2a97a73de32ca762dec332c85cb01fe764 127.0.0.1:7003@17003 myself,master - 0 1579016072000 0 connected
    30694696cd31aaca79d71e9e7ad8ce62f471130d 127.0.0.1:7002@17002 master - 0 1579016071541 3 connected 10923-16383

And set it as a slave of node `127.0.0.1:7000`

    127.0.0.1:7003> CLUSTER REPLICATE 4e1901ce95cfb749b94c435e1f1c123ae0579e79
    OK
    127.0.0.1:7003> CLUSTER NODES
    fd1099f4aff0be7fb014e1473c404631a764ffa4 127.0.0.1:7001@17001 master - 0 1579016555553 2 connected 5461-9188 9190-10922
    4e1901ce95cfb749b94c435e1f1c123ae0579e79 127.0.0.1:7000@17000 master - 0 1579016556680 4 connected 0-5460 9189
    a9b6ad2a97a73de32ca762dec332c85cb01fe764 127.0.0.1:7003@17003 myself,slave 4e1901ce95cfb749b94c435e1f1c123ae0579e79 0 1579016555000 0 connected
    30694696cd31aaca79d71e9e7ad8ce62f471130d 127.0.0.1:7002@17002 master - 0 1579016555652 3 connected 10923-16383


If we fail the node `127.0.0.1:7000`

    ^C943:signal-handler (1579016631) Received SIGINT scheduling shutdown...
    943:M 14 Jan 2020 23:43:51.330 # User requested shutdown...
    943:M 14 Jan 2020 23:43:51.330 * Calling fsync() on the AOF file.
    943:M 14 Jan 2020 23:43:51.330 # Redis is now ready to exit, bye bye...
    zpoint@bogon 7000 %

We can find that the node `127.0.0.1:7003` becomes master and the epoch num is the greatest 5

    127.0.0.1:7003> CLUSTER NODES
    fd1099f4aff0be7fb014e1473c404631a764ffa4 127.0.0.1:7001@17001 master - 0 1579016657320 2 connected 5461-9188 9190-10922
    4e1901ce95cfb749b94c435e1f1c123ae0579e79 127.0.0.1:7000@17000 master,fail - 1579016631436 1579016629000 4 disconnected
    a9b6ad2a97a73de32ca762dec332c85cb01fe764 127.0.0.1:7003@17003 myself,master - 0 1579016656000 5 connected 0-5460 9189
    30694696cd31aaca79d71e9e7ad8ce62f471130d 127.0.0.1:7002@17002 master - 0 1579016658337 3 connected 10923-16383