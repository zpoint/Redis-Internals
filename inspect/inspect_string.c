//
// Created by zpoint on 2019-07-20.
//
#pragma once
#include "util.h"



void inspect_int(const sds s)
{
    fprintf(REDIS_INSPECT_STD_OUT, "long val of o->ptr: %ld\n", (long)s);
}


void inspect_normal_string(const sds s)
{
    if (s == NULL)
    {
        fprintf(REDIS_INSPECT_STD_OUT, "call inspect_normal_string, but s is NULL, goint to return\n");
        return;
    }
    void *sds_ptr = (void *)s;
    unsigned char flags = s[-1];
    fprintf(REDIS_INSPECT_STD_OUT, "addr of flags: %p, value in flags: %hhx, value of flags&SDS_TYPE_MASK: %x\n", (void *)&s[-1], flags, flags&SDS_TYPE_MASK);
    switch(flags&SDS_TYPE_MASK) {
        case SDS_TYPE_5:
            sds_ptr = (struct sdshdr5 *)((s)-(sizeof(struct sdshdr5)));
            fprintf(REDIS_INSPECT_STD_OUT, "sdshdr5, (flags)>>SDS_TYPE_BITS: %x, addr of o->buf[0]: %p\n", (flags)>>SDS_TYPE_BITS, (void *)&(((struct sdshdr5 *)sds_ptr)->buf[0]));
            for (int i = 0; i < (flags)>>SDS_TYPE_BITS; ++i)
            {
                fprintf(REDIS_INSPECT_STD_OUT, "addr of buf[%d]: %p, value in buf[%d]: %hhx(%c)\n", i, (void *)&(((struct sdshdr5 *)sds_ptr)->buf[i]), i, ((struct sdshdr5 *)sds_ptr)->buf[i], ((struct sdshdr5 *)sds_ptr)->buf[i]);
            }
            break;
        case SDS_TYPE_8:
            sds_ptr = (struct sdshdr8 *)((s)-(sizeof(struct sdshdr8)));
            fprintf(REDIS_INSPECT_STD_OUT, "sdshdr8, addr of sds->len: %p, value in sds->len: 0x%hhx, addr of sds->alloc: %p, value in sds->alloc: 0x%hhx, addr of o->buf: %p, value in o->buf: %p\n", (void *)&(((struct sdshdr8 *)sds_ptr)->len), ((struct sdshdr8 *)sds_ptr)->len, (void *)&(((struct sdshdr8 *)sds_ptr)->alloc), ((struct sdshdr8 *)sds_ptr)->alloc, (void *)&(((struct sdshdr8 *)sds_ptr)->buf), (void *)(((struct sdshdr8 *)sds_ptr)->buf));
            for (int i = 0; i < ((struct sdshdr8 *)sds_ptr)->len; ++i)
            {
                fprintf(REDIS_INSPECT_STD_OUT, "addr of buf[%d]: %p, value in buf[%d]: 0x%hhx(%c)\n", i, (void *)&(((struct sdshdr8 *)sds_ptr)->buf[i]), i, ((struct sdshdr8 *)sds_ptr)->buf[i], ((struct sdshdr8 *)sds_ptr)->buf[i]);
            }
            break;
        case SDS_TYPE_16:
            sds_ptr = (struct sdshdr16 *)((s)-(sizeof(struct sdshdr16)));
            fprintf(REDIS_INSPECT_STD_OUT, "sdshdr16, addr of sds->len: %p, value in sds->len: 0x%hx, addr of sds->alloc: %p, value in sds->alloc: 0x%hx, addr of o->buf: %p, value in o->buf: %p\n", (void *)&(((struct sdshdr16 *)sds_ptr)->len), ((struct sdshdr16 *)sds_ptr)->len, (void *)&(((struct sdshdr16 *)sds_ptr)->alloc), ((struct sdshdr16 *)sds_ptr)->alloc, (void *)&(((struct sdshdr16 *)sds_ptr)->buf), (void *)(((struct sdshdr16 *)sds_ptr)->buf));
            for (int i = 0; i < ((struct sdshdr16 *)sds_ptr)->len; ++i)
            {
                fprintf(REDIS_INSPECT_STD_OUT, "addr of buf[%d]: %p, value in buf[%d]: 0x%hhx(%c)\n", i, (void *)&(((struct sdshdr16 *)sds_ptr)->buf[i]), i, ((struct sdshdr16 *)sds_ptr)->buf[i], ((struct sdshdr16 *)sds_ptr)->buf[i]);
            }
            break;
        case SDS_TYPE_32:
            sds_ptr = (struct sdshdr32 *)((s)-(sizeof(struct sdshdr32)));
            fprintf(REDIS_INSPECT_STD_OUT, "sdshdr32, addr of sds->len: %p, value in sds->len: 0x%x, addr of sds->alloc: %p, value in sds->alloc: 0x%x, addr of o->buf: %p, value in o->buf: %p\n", (void *)&(((struct sdshdr32 *)sds_ptr)->len), ((struct sdshdr32 *)sds_ptr)->len, (void *)&(((struct sdshdr32 *)sds_ptr)->alloc), ((struct sdshdr32 *)sds_ptr)->alloc, (void *)&(((struct sdshdr32 *)sds_ptr)->buf), (void *)(((struct sdshdr32 *)sds_ptr)->buf));
            for (uint32_t i = 0; i < ((struct sdshdr32 *)sds_ptr)->len; ++i)
            {
                fprintf(REDIS_INSPECT_STD_OUT, "addr of buf[%d]: %p, value in buf[%d]: 0x%hhx(%c)\n", i, (void *)&(((struct sdshdr32 *)sds_ptr)->buf[i]), i, ((struct sdshdr32 *)sds_ptr)->buf[i], ((struct sdshdr32 *)sds_ptr)->buf[i]);
            }
            break;
        case SDS_TYPE_64:
            sds_ptr = (struct sdshdr64 *)((s)-(sizeof(struct sdshdr64)));
            fprintf(REDIS_INSPECT_STD_OUT, "sdshdr64, addr of sds->len: %p, value in sds->len: 0x%llx, addr of sds->alloc: %p, value in sds->alloc: 0x%llx, addr of o->buf: %p, value in o->buf: %p\n", (void *)&(((struct sdshdr64 *)sds_ptr)->len), ((struct sdshdr64 *)sds_ptr)->len, (void *)&(((struct sdshdr64 *)sds_ptr)->alloc), ((struct sdshdr64 *)sds_ptr)->alloc, (void *)&(((struct sdshdr64 *)sds_ptr)->buf), (void *)(((struct sdshdr64 *)sds_ptr)->buf));
            for (uint64_t i = 0; i < ((struct sdshdr64 *)sds_ptr)->len; ++i)
            {
                fprintf(REDIS_INSPECT_STD_OUT, "addr of buf[%llu]: %p, value in buf[%llu]: 0x%hhx(%c)\n", i, (void *)&(((struct sdshdr64 *)sds_ptr)->buf[i]), i, ((struct sdshdr64 *)sds_ptr)->buf[i], ((struct sdshdr64 *)sds_ptr)->buf[i]);
            }
            break;
        default:
            fprintf(REDIS_INSPECT_STD_OUT, "unknown flags\n");
    }
    fprintf(REDIS_INSPECT_STD_OUT, "sds val: %s\n", (char *)s);
}


void inspect_string(robj *o)
{
    super_inspect(o);
    if (o->encoding == OBJ_ENCODING_INT)
        inspect_int(o->ptr);
    else
        inspect_normal_string(o->ptr);
}
