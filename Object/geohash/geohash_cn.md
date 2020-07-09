# geohash![image title](http://www.zpoint.xyz:8080/count/tag.svg?url=github%2FRedis-Internals%2Fgeohash_cn)

# 目录

* [相关位置文件](#相关位置文件)
* [内存构造](#内存构造)
* [encode](#encode)
* [decode](#decode)
* [geoadd](#geoadd)
* [geohash](#geohash)
* [更多资料](#更多资料)

# 相关位置文件
* redis/src/geohash.c
* redis/src/geohash.h
* redis/src/geo.c
* redis/src/geohash_helper.c

# 内存构造

![geo_hash](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/geohash/geo_hash.png)



我们以一个坐标点 `113.936698, 22.543764`为例进行示范

# encode

```c
/* Limits from EPSG:900913 / EPSG:3785 / OSGEO:41001 */
#define GEO_LAT_MIN -85.05112878
#define GEO_LAT_MAX 85.05112878
#define GEO_LONG_MIN -180
#define GEO_LONG_MAX 180
```

你能进行存储的纬度的最大值/最小值是 85.05/-85.05, 也就是北极/南极附近的范围

这是一部分 encode 的代码

```c
int geohashEncode(...)
{
  /* 忽略 */
  double lat_offset =
      (latitude - lat_range->min) / (lat_range->max - lat_range->min);
  double long_offset =
      (longitude - long_range->min) / (long_range->max - long_range->min);

  /* 根据设置的step大小, 转换为一个固定的值 */
  lat_offset *= (1ULL << step);
  long_offset *= (1ULL << step);
  hash->bits = interleave64(lat_offset, long_offset);
  return 1;
}
```

![lat_offset](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/geohash/lat_offset.png)

![lon_offset](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/geohash/lon_offset.png)

在我们的示例中, lat_offset 大小为 `(22.543764 - (-85.05112878))/(85.05112878 - (-85.05112878))` 也就是  `0.632531`

lat_offset 的大小为 `(113.936698 - (-180))/(180 - (-180))` 也就是 `0.816491`

默认的 `step` 大小为 `26`, 所以最后一行实际传入的值为 `hash->bits = interleave64(42448413, 54793771); `

```c
static inline uint64_t interleave64(uint32_t xlo, uint32_t ylo) {
    static const uint64_t B[] = {0x5555555555555555ULL, 0x3333333333333333ULL,
                                 0x0F0F0F0F0F0F0F0FULL, 0x00FF00FF00FF00FFULL,
                                 0x0000FFFF0000FFFFULL};
    static const unsigned int S[] = {1, 2, 4, 8, 16};
    uint64_t x = xlo;
    uint64_t y = ylo;
    // step1
    x = (x | (x << S[4])) & B[4];
    y = (y | (y << S[4])) & B[4];
    // step2
    x = (x | (x << S[3])) & B[3];
    y = (y | (y << S[3])) & B[3];
    // step3
    x = (x | (x << S[2])) & B[2];
    y = (y | (y << S[2])) & B[2];
    // step4
    x = (x | (x << S[1])) & B[1];
    y = (y | (y << S[1])) & B[1];
    // step5
    x = (x | (x << S[0])) & B[0];
    y = (y | (y << S[0])) & B[0];

    return x | (y << 1);
}
```

我们来看看 `interleave64` 在 encoding 这一步中做了什么

这是`step1 `之后的  `x` 和 `y` 的值

![step1](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/geohash/step1.png)

这是  `step2` 之后的  `x` 和 `y` 的值

![step2](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/geohash/step2.png)

在  `step2` 之后, 原本的连续的4个字节(32个bit)被连续均匀的按照1个字节(8个bits)为一组分布在新的64bits大小的容器中

这是  `step3` 之后的  `x` 和 `y` 的值

![step3](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/geohash/step3.png) 

现在我们可以发现, 在每一个 `step3`  中, 连续的字节/比特会按照不同的颗粒度来进行重新分布

这是  `step4` 之后的  `x` 和 `y` 的值

![step4](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/geohash/step4.png)

现在的颗粒度大小为 2bits(一组 2个bits)

最后的一个 `step` 把每一个原本的32位的 bit 都均匀的分布在当前的容器内, 在一幅图中按bit为单位画出来太繁琐了, 所以我们就忽略 `step5` 的图像好了

最后的  `x | (y << 1)` 把 x 和 y 交错在一起, 示例如下

![interleave](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/geohash/interleave.png)

# decode

decode 的过程和  [encode](#encode) 的过程类似, 只需要把左移变成右移, 乘变成除

# geoadd

```shell script
127.0.0.1:6379> geoadd my_key 113.936698 22.543764 my_location
(integer) 1
```

[encode](#encode) 之后的十进制值为 `4046431618599387`, 在 `encode`之后, `geoadd` 会调用 [zadd](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/zset/zset_cn.md#OBJ_ENCODING_ZIPLIST)  命令对 `encode` 之后的值进行存储, 存储完之后的样子如图所示

![geoadd_ziplist](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/geohash/geoadd_ziplist.png)

# geohash

```shell script
127.0.0.1:6379> geohash my_key my_location
1) "ws100xynvq0"
```

这是源代码中的一部分

```c
char *geoalphabet= "0123456789bcdefghjkmnpqrstuvwxyz";
char buf[12];
int i;
for (i = 0; i < 11; i++) {
    int idx = (hash.bits >> (52-((i+1)*5))) & 0x1f;
    buf[i] = geoalphabet[idx];
}
buf[11] = '\0';
```

这个算法首先获取到 [encode](#encode) 之后的值, 从第 13个 bit 开始, 每 5个 bits 为一组, 获取到对应组的 base32 的值

``` 
0000 0000 0000 |1110 0|110 0000 0010 0000 0000 0111 0111 1101 0100 1101 1101 1010
                  w
0000 0000 0000 1110 0|110 00|00 0010 0000 0000 0111 0111 1101 0100 1101 1101 1010
                        s
0000 0000 0000 1110 0110 00|00 001|0 0000 0000 0111 0111 1101 0100 1101 1101 1010
                              1
0000 0000 0000 1110 0110 0000 001|0 0000| 0000 0111 0111 1101 0100 1101 1101 1010
                                    0
0000 0000 0000 1110 0110 0000 0010 0000| 0000 0|111 0111 1101 0100 1101 1101 1010
                                           0
0000 0000 0000 1110 0110 0000 0010 0000 0000 0|111 01|11 1101 0100 1101 1101 1010
                                                 x
0000 0000 0000 1110 0110 0000 0010 0000 0000 0111 01|11 110|1 0100 1101 1101 1010
                                                        y
0000 0000 0000 1110 0110 0000 0010 0000 0000 0111 0111 110|1 0100| 1101 1101 1010
                                                             n
0000 0000 0000 1110 0110 0000 0010 0000 0000 0111 0111 1101 0100| 1101 1|101 1010
                                                                    v
0000 0000 0000 1110 0110 0000 0010 0000 0000 0111 0111 1101 0100 1101 1|101 10|10
                                                                           q
0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 000|0 0000|
                                                                            0
```

最后一组负数位移可以参考 [shifting-with-a-negative-shift-count](https://stackoverflow.com/questions/4945703/left-shifting-with-a-negative-shift-count)

相近的坐标点的 geohash 结果的前缀会是相同的, 我们可以把精度要求不是非常高的场景中的坐标点的查找变成字符串前缀查找, 比如附近的人/位置

# 更多资料

* [Redis原理及实践之GeoHash](https://www.jianshu.com/p/c9801c4f9f6a)
* [GeoHash核心原理解析](https://www.cnblogs.com/LBSer/p/3310455.html)

