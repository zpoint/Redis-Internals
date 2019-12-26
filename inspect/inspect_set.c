//
// Created by zpoint on 2019-08-14.
//

#pragma once
#include "util.h"
#include "inspect_dict.c"
#include "../src/intset.h"

#define INTSET_ENC_INT16 (sizeof(int16_t))
#define INTSET_ENC_INT32 (sizeof(int32_t))
#define INTSET_ENC_INT64 (sizeof(int64_t))

void inspect_intset(intset *intset_ptr)
{
    fprintf(REDIS_INSPECT_STD_OUT, "intset_ptr->encoding: %u", intset_ptr->encoding);
    if (intset_ptr->encoding == INTSET_ENC_INT64)
        fprintf(REDIS_INSPECT_STD_OUT, "(%s)", "INTSET_ENC_INT64");
    else if (intset_ptr->encoding == INTSET_ENC_INT32)
        fprintf(REDIS_INSPECT_STD_OUT, "(%s)", "INTSET_ENC_INT32");
    else if (intset_ptr->encoding == INTSET_ENC_INT16)
        fprintf(REDIS_INSPECT_STD_OUT, "(%s)", "INTSET_ENC_INT16");
    else
        fprintf(REDIS_INSPECT_STD_OUT, "(%s)", "UNKNOWN INSET ENCODING");
    fprintf(REDIS_INSPECT_STD_OUT, "addr of intset_ptr->encoding: %p\n", (void *)&intset_ptr->encoding);

    fprintf(REDIS_INSPECT_STD_OUT, "intset_ptr->length: %u, addr of intset_ptr->length: %p\n", intset_ptr->length, (void *)&intset_ptr->length);
    size_t index = 0;
    for (size_t i = 0; i < intset_ptr->length; ++i)
    {
        fprintf(REDIS_INSPECT_STD_OUT, "index: %lu, addr: %p, value: ", i, (void *)&intset_ptr->contents[index]);
        if (intset_ptr->encoding == INTSET_ENC_INT64)
        {
            fprintf(REDIS_INSPECT_STD_OUT, "%lld ", *((long long *)&intset_ptr->contents[index]));
            index += INTSET_ENC_INT64;
        }
        else if (intset_ptr->encoding == INTSET_ENC_INT32)
        {
            fprintf(REDIS_INSPECT_STD_OUT, "%d ", *((int *)&intset_ptr->contents[index]));
            index += INTSET_ENC_INT32;
        }
        else
        {
            fprintf(REDIS_INSPECT_STD_OUT, "%hd ", *((short *)&intset_ptr->contents[index]));
            index += INTSET_ENC_INT16;
        }
        // pr_binary_16(intset_ptr->contents[index - INTSET_ENC_INT16]);
        fprintf(REDIS_INSPECT_STD_OUT, "\n");
    }
    /*
    for (size_t i = 0; i < intset_ptr->length * 2; ++i)
    {
        pr_binary(intset_ptr->contents[i]);
    }
    */
}

void inspect_set(robj *o)
{
    if (o->encoding == OBJ_ENCODING_INTSET)
    {
        inspect_intset((intset *)o->ptr);
    }
    else if (o->encoding == OBJ_ENCODING_HT)
        inspect_dict((dict *)o->ptr);
    else
        fprintf(REDIS_INSPECT_STD_OUT, "unknown encoding: %d\n", o->encoding);
}
