//
// Created by zpoint on 2019-08-02.
//

#pragma once

#include "../src/server.h"
#include "util.h"
#include "inspect_string.c"
#include "inspect_dict.c"
#include "inspect_list.c"
#include "inspect_set.c"
#include "inspect_zset.c"
#include "inspect_hyperloglog.c"
#include "inspect_streams.c"

void inspect(robj *o)
{
    fprintf(REDIS_INSPECT_STD_OUT, "\n\ninspect begin, addr of o: %p\n", (void *)o);
    switch (o->type)
    {
        case OBJ_STRING:
            fprintf(REDIS_INSPECT_STD_OUT, "type: %d, %s\n", o->type, "OBJ_STRING");
            inspect_string(o);
            break;
        case OBJ_LIST:
            fprintf(REDIS_INSPECT_STD_OUT, "type: %d, %s\n", o->type, "OBJ_LIST");
            inspect_list(o);
            break;
        case OBJ_SET:
            fprintf(REDIS_INSPECT_STD_OUT, "type: %d, %s\n", o->type, "OBJ_SET");
            inspect_set(o);
            break;
        case OBJ_ZSET:
            fprintf(REDIS_INSPECT_STD_OUT, "type: %d, %s\n", o->type, "OBJ_ZSET");
            inspect_zset(o);
            break;
        case OBJ_HASH:
            fprintf(REDIS_INSPECT_STD_OUT, "type: %d, %s\n", o->type, "OBJ_HASH");
            inspect_hash(o);
            break;
        case OBJ_MODULE:
            fprintf(REDIS_INSPECT_STD_OUT, "type: %d, %s\n", o->type, "OBJ_MODULE");
            break;
        case OBJ_STREAM:
            fprintf(REDIS_INSPECT_STD_OUT, "type: %d, %s\n", o->type, "OBJ_STREAM");
            inspect_streams(o);
            break;
        case REDISMODULE_TYPE_ENCVER_BITS:
            fprintf(REDIS_INSPECT_STD_OUT, "type: %d, %s\n", o->type, "REDISMODULE_TYPE_ENCVER_BITS");
            break;
        default:
            fprintf(REDIS_INSPECT_STD_OUT, "unknown type\n");
    }

    switch (o->encoding)
    {
        case OBJ_ENCODING_RAW:
            fprintf(REDIS_INSPECT_STD_OUT, "encoding: %d, %s\n", o->encoding, "OBJ_ENCODING_RAW");
            break;
        case OBJ_ENCODING_INT:
            fprintf(REDIS_INSPECT_STD_OUT, "encoding: %d, %s\n", o->encoding, "OBJ_ENCODING_INT");
            break;
        case OBJ_ENCODING_HT:
            fprintf(REDIS_INSPECT_STD_OUT, "encoding: %d, %s\n", o->encoding, "OBJ_ENCODING_HT");
            break;
        case OBJ_ENCODING_ZIPMAP:
            fprintf(REDIS_INSPECT_STD_OUT, "encoding: %d, %s\n", o->encoding, "OBJ_ENCODING_ZIPMAP");
            break;
        case OBJ_ENCODING_LINKEDLIST:
            fprintf(REDIS_INSPECT_STD_OUT, "encoding: %d, %s\n", o->encoding, "OBJ_ENCODING_LINKEDLIST");
            break;
        case OBJ_ENCODING_ZIPLIST:
            fprintf(REDIS_INSPECT_STD_OUT, "encoding: %d, %s\n", o->encoding, "OBJ_ENCODING_ZIPLIST");
            break;
        case OBJ_ENCODING_INTSET:
            fprintf(REDIS_INSPECT_STD_OUT, "encoding: %d, %s\n", o->encoding, "OBJ_ENCODING_INTSET");
            break;
        case OBJ_ENCODING_SKIPLIST:
            fprintf(REDIS_INSPECT_STD_OUT, "encoding: %d, %s\n", o->encoding, "OBJ_ENCODING_SKIPLIST");
            break;
        case OBJ_ENCODING_EMBSTR:
            fprintf(REDIS_INSPECT_STD_OUT, "encoding: %d, %s\n", o->encoding, "OBJ_ENCODING_EMBSTR");
            break;
        case OBJ_ENCODING_QUICKLIST:
            fprintf(REDIS_INSPECT_STD_OUT, "encoding: %d, %s\n", o->encoding, "OBJ_ENCODING_QUICKLIST");
            break;
        case OBJ_ENCODING_STREAM:
            fprintf(REDIS_INSPECT_STD_OUT, "encoding: %d, %s\n", o->encoding, "OBJ_ENCODING_STREAM");
            break;
        default:
            fprintf(REDIS_INSPECT_STD_OUT, "unknown encoding: %d\n", o->encoding);
    }
    printf("lru: %x\n", o->lru);
    printf("refcount: %d, addr of refcount: %p\n", o->refcount, (void *)&o->refcount);
    printf("ptr value: %p, address of ptr: %p\n", o->ptr, (void *)&o->ptr);
    printf("inspect end\n\n");
}

