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

![lat_offset](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/geohash/lat_offset.png)

![lon_offset](https://github.com/zpoint/Redis-Internals/blob/5.0/Object/geohash/lon_offset.png)

# decode

# geoadd

```shell script
127.0.0.1:6379> geoadd my_key 113.936698 22.543764 my_location
(integer) 1
```

# read more

* [Redis原理及实践之GeoHash](https://www.jianshu.com/p/c9801c4f9f6a)