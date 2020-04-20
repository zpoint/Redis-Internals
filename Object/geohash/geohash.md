# geohash

# contents

* [related file](#related-file)
* [memory layout](#memory-layout)
* [read more](#read-more)

# related file
* redis/src/geohash.c
* redis/src/geohash.h
* redis/src/geo.c

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

The maximum value you can encode for a  LAT/LON is 85.05, which is the north/south pole

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

The default `step` is `26`, so the last line will be `hash->bits = interleave64(42448413.855534, 54793771.918586); `

# decode

# geoadd

```shell script
127.0.0.1:6379> geoadd my_key 113.936698 22.543764 my_location
(integer) 1
```

# read more

* [Redis原理及实践之GeoHash](https://www.jianshu.com/p/c9801c4f9f6a)
* [GeoHash核心原理解析](https://www.cnblogs.com/LBSer/p/3310455.html)