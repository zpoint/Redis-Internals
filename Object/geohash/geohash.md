# geohash![image title](http://www.zpoint.xyz:8080/count/tag.svg?url=github%2FRedis-Internals%2Fgeohash)

# contents

* [related file](#related-file)
* [memory layout](#memory-layout)
* [encode](#encode)
* [decode](#decode)
* [geoadd](#geoadd)
* [geohash](#geohash)
* [read more](#read-more)

# related file
* redis/src/geohash.c
* redis/src/geohash.h
* redis/src/geo.c
* redis/src/geohash_helper.c

# memory layout

![geo_hash](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/geohash/geo_hash.png)



Let's begin with an example geo point `113.936698, 22.543764`

# encode

```c
/* Limits from EPSG:900913 / EPSG:3785 / OSGEO:41001 */
#define GEO_LAT_MIN -85.05112878
#define GEO_LAT_MAX 85.05112878
#define GEO_LONG_MIN -180
#define GEO_LONG_MAX 180
```

The maximum value you can encode for a  LAT is 85.05/-85.05, which is the north/south pole

This is part of the encode code

```c
int geohashEncode(...)
{
  /* omit */
  double lat_offset =
      (latitude - lat_range->min) / (lat_range->max - lat_range->min);
  double long_offset =
      (longitude - long_range->min) / (long_range->max - long_range->min);

  /* convert to fixed point based on the step size */
  lat_offset *= (1ULL << step);
  long_offset *= (1ULL << step);
  hash->bits = interleave64(lat_offset, long_offset);
  return 1;
}
```

![lat_offset](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/geohash/lat_offset.png)

![lon_offset](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/geohash/lon_offset.png)

In our example, lat_offset is `(22.543764 - (-85.05112878))/(85.05112878 - (-85.05112878))` which is `0.632531`

lon_offset is `(113.936698 - (-180))/(180 - (-180))` which is `0.816491`

The default `step` is `26`, so the last line will be `hash->bits = interleave64(42448413, 54793771); `

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

Let's figure out what `interleave64` did in encoding

This is the value of `x` and `y` after `step1`

![step1](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/geohash/step1.png)

The value of `x` and `y` after `step2`

![step2](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/geohash/step2.png)

After `step2`, the origin contiguous 4 bytes(32 bits) is evenly dirstributed in the new 8 bytes(64 bit) container(8bits per group)

The value of `x` and `y` after `step3`

![step3](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/geohash/step3.png) 

Now, we can learn that in each step, the contiguous bytes/bits is seperated in different granularity

The value of `x` and `y` after `step4`

![step4](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/geohash/step4.png)

The granularity becomes 2 bits

The final step seperates every single bit in the origin 32 bit integer, it's too large to fit into a single image, so we will ignore `step5`

And the final statement `x | (y << 1)` interleaves x and y

![interleave](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/geohash/interleave.png)

# decode

The decode procedure is similar to the [encode](#encode) procedure, just reverse the left shift to right shift, and multiply to divide

# geoadd

```shell script
127.0.0.1:6379> geoadd my_key 113.936698 22.543764 my_location
(integer) 1
```

The decimal interleaved result after [encode](#encode) is `4046431618599387`,  after `encode`, `geoadd` will delegate the command to [zadd](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/zset/zset.md#OBJ_ENCODING_ZIPLIST), which becomes

![geoadd_ziplist](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/geohash/geoadd_ziplist.png)

# geohash

```shell script
127.0.0.1:6379> geohash my_key my_location
1) "ws100xynvq0"
```

This is part of source code

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

The algorithm first get the encoded result, begin from the 13th bit, group every 5 bits together and get the base32 value of the bit group

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

# read more

About [shifting-with-a-negative-shift-count](https://stackoverflow.com/questions/4945703/left-shifting-with-a-negative-shift-count)

Those coordinates in near location will have same geohash string prefix. We can turn a coordinate search to a string prefix search in the scenario which does not requiring high accuracy result such as neraby people/location

# read more

* [Redis原理及实践之GeoHash](https://www.jianshu.com/p/c9801c4f9f6a)
* [GeoHash核心原理解析](https://www.cnblogs.com/LBSer/p/3310455.html)