//
// Created by zpoint on 2019-07-20.
//

#pragma once
// set to stdout
#define REDIS_INSPECT_STD_OUT stdout

void pr_binary(char val)
{
    for (int i = sizeof(val) * 8 - 1; i >= 0; i --)
    {
        char tmp_val = val >> i;
        fprintf(REDIS_INSPECT_STD_OUT, "%d", tmp_val & 1);
        if (i % 4 == 0)
            fprintf(REDIS_INSPECT_STD_OUT, " ");
    }
}

void pr_binary_64(uint64_t val)
{
    char *ptr = (char *)&val;
    fprintf(REDIS_INSPECT_STD_OUT, "uint64_t type, decimal value: %llu, binary: ", val);
    for (size_t i = 0; i < sizeof(val); ++i, ++ptr)
    {
        pr_binary(*ptr);
    }
}

void pr_binary_32(uint32_t val)
{
    char *ptr = (char *)&val;
    fprintf(REDIS_INSPECT_STD_OUT, "uint32_t type, decimal value: %d, binary: ", val);
    for (size_t i = 0; i < sizeof(val); ++i, ++ptr)
    {
        pr_binary(*ptr);
    }
}

void pr_binary_16(uint16_t val)
{
    char *ptr = (char *)&val;
    fprintf(REDIS_INSPECT_STD_OUT, "uint16_t type, decimal value: %d, binary: ", val);
    for (size_t i = 0; i < 2; ++i, ++ptr)
    {
        pr_binary(*ptr);
    }
}

void super_inspect(robj *o)
{
    fprintf(REDIS_INSPECT_STD_OUT, "addr of o: %p\n", (void *)o);
    char *char_ptr = (char *)o;
    fprintf(REDIS_INSPECT_STD_OUT, "traversing\n");
    while (char_ptr < (char *)(&o->refcount))
    {
        fprintf(REDIS_INSPECT_STD_OUT, "addr of ptr: %p, value in ptr: %hhx, binary value: ", (void *)char_ptr, *char_ptr);
        pr_binary(*char_ptr);
        fprintf(REDIS_INSPECT_STD_OUT, "\n");
        char_ptr += 1;
    }

    fprintf(REDIS_INSPECT_STD_OUT, "addr of o->refcount: %p, value in o->refcount: %d\n", (void *)&o->refcount, o->refcount);
    fprintf(REDIS_INSPECT_STD_OUT, "addr of o->ptr: %p, value in o->ptr: %p\n", (void *)&o->ptr, (void *)o->ptr);
}
