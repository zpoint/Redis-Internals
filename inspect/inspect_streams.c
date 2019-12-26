//
// Created by zpoint on 2019/11/30.
//

#pragma once
#include "util.h"
#include "inspect_rax.c"
#include "inspect_listpack.h"
#include "../src/stream.h"

void pr_ptr_node_normal(raxNode *ptr_node)
{
    for (size_t i = 0; i < ptr_node->size; ++i)
    {
        fprintf(REDIS_INSPECT_STD_OUT, "%c(0x%hhx) ", ptr_node->data[i], ptr_node->data[i]);
    }
}

void pr_ptr_node_stream(raxNode *ptr_node)
{
    unsigned char tmp = 0;
    unsigned long long val;
    unsigned char* ptr_2_val = (unsigned char *)&val;
    size_t mid = 0, max_size = 0;
    /* store in network(big endian) order, need to revert */
    if (ptr_node->size == 16)
    {
        pr_ptr_node_normal(ptr_node);
        for (size_t i = 0; i < ptr_node->size; i += sizeof(unsigned long long))
        {
            val = *((unsigned long long *)(&ptr_node->data[i]));

            mid = sizeof(unsigned long long) / 2;
            max_size = sizeof(unsigned long long);
            fprintf(REDIS_INSPECT_STD_OUT, "\nbefore i: %lu, %llu(0x%llx) ", i, val, val);
            for (size_t z = 0; z < mid; ++z)
            {
                tmp = ptr_2_val[z];
                ptr_2_val[z] = ptr_2_val[max_size - z - 1];
                ptr_2_val[max_size - z - 1] = tmp;
            }
            fprintf(REDIS_INSPECT_STD_OUT, "\ni: %lu, %llu(0x%llx) ", i, val, val);
        }
    }
    else
    {
        /* compressed node or uncompressed node but split */
        fprintf(REDIS_INSPECT_STD_OUT, "\nptr_node->size: %d\n", ptr_node->size);
        for (size_t i = 0; i < ptr_node->size; ++i)
        {
            fprintf(REDIS_INSPECT_STD_OUT, "(0x%hhx) ", ptr_node->data[i]);
        }
        fprintf(REDIS_INSPECT_STD_OUT, "\n");
    }
}

void inspect_stream_nack(void *na)
{
    streamNACK *na_new = (streamNACK *)na;
    fprintf(REDIS_INSPECT_STD_OUT, "address of streamNACK: %p\n", (void *)na_new);
    fprintf(REDIS_INSPECT_STD_OUT, "na->delivery_time: %lld\n", na_new->delivery_time);
    fprintf(REDIS_INSPECT_STD_OUT, "na->delivery_count: %llu\n", na_new->delivery_count);
    fprintf(REDIS_INSPECT_STD_OUT, "address of na->consumer: %p\n", (void *)na_new->consumer);
}

void inspect_stream_consumer(void *sc)
{
    streamConsumer *sc_new = (streamConsumer *)sc;
    fprintf(REDIS_INSPECT_STD_OUT, "address of streamConsumer: %p\n", (void *)sc_new);
    fprintf(REDIS_INSPECT_STD_OUT, "sc->seen_time: %lld\n", sc_new->seen_time);
    fprintf(REDIS_INSPECT_STD_OUT, "sc->name: %s\n", sc_new->name);
    fprintf(REDIS_INSPECT_STD_OUT, "sc->pel: %p\n", (void *)sc_new->pel);
    fprintf(REDIS_INSPECT_STD_OUT, "\ngoing to inspect stream_consumer rax* pel\n");
    inspect_rax(sc_new->pel, inspect_stream_nack, pr_ptr_node_stream);
}

void inspect_stream_cg(void *cg)
{
    streamCG *cg_new = (streamCG *)cg;
    fprintf(REDIS_INSPECT_STD_OUT, "streamCG: %p\n", cg);
    fprintf(REDIS_INSPECT_STD_OUT, "last_id.ms: %llu, last_id.seq: %llu\n", cg_new->last_id.ms, cg_new->last_id.seq);
    fprintf(REDIS_INSPECT_STD_OUT, "\ngoing to inspect rax* pel\n");
    inspect_rax(cg_new->pel, inspect_stream_nack, pr_ptr_node_stream);
    fprintf(REDIS_INSPECT_STD_OUT, "\ngoing to inspect rax* consumers\n");
    inspect_rax(cg_new->consumers, inspect_stream_consumer, pr_ptr_node_normal);
}

void inspect_streams(robj *o)
{
    stream *s = o->ptr;
    rax *rax = s->rax, *cg = s->cgroups;
    fprintf(REDIS_INSPECT_STD_OUT, "rax s->rax: %p\n", (void *)rax);
    fprintf(REDIS_INSPECT_STD_OUT, "stream s->length: %llu\n", s->length);
    fprintf(REDIS_INSPECT_STD_OUT, "s->last_id.ms: %llu, s->last_id.seq: %llu\n", s->last_id.ms, s->last_id.seq);
    fprintf(REDIS_INSPECT_STD_OUT, "rax s->cgroups: %p\n", (void *)cg);

    fprintf(REDIS_INSPECT_STD_OUT, "\ngoing to inspect rax\n");
    inspect_rax(rax, inspect_listpack, pr_ptr_node_stream);
    fprintf(REDIS_INSPECT_STD_OUT, "\ngoing to inspect consumer group\n");
    inspect_rax(cg, inspect_stream_cg, pr_ptr_node_normal);
}

