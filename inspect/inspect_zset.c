//
// Created by zpoint on 2019-08-19.
//

#pragma once
#include "util.h"
#include "inspect_dict.c"
#include "../src/server.h"

void inspect_zskiplist(zskiplist *zskiplist_ptr)
{
    fprintf(REDIS_INSPECT_STD_OUT, "\n\nzskiplist_ptr: %p\n", (void *)zskiplist_ptr);
    fprintf(REDIS_INSPECT_STD_OUT, "header: %p, tail: %p, length: %lu, level: %d\n", (void *)zskiplist_ptr->header, (void *)zskiplist_ptr->tail, zskiplist_ptr->length, zskiplist_ptr->level);
    zskiplistNode *node = zskiplist_ptr->header;
    struct zskiplistLevel *level_ptr = NULL;
    int index = 0;
    /* header */
    fprintf(REDIS_INSPECT_STD_OUT, "header: %p\n", (void *)node);
    fprintf(REDIS_INSPECT_STD_OUT, "header->ele: ");
    inspect_normal_string(node->ele);
    fprintf(REDIS_INSPECT_STD_OUT, "header->score: %lf\n", node->score);
    fprintf(REDIS_INSPECT_STD_OUT, "header->backward: %p\n", (void *)node->backward);
    index = 0;
    while (index < ZSKIPLIST_MAXLEVEL)
    {
        fprintf(REDIS_INSPECT_STD_OUT, "header->level[%d].forward: %p\n", index, (void *)node->level[index].forward);
        fprintf(REDIS_INSPECT_STD_OUT, "header->level[%d].span: %lu\n", index, node->level[index].span);
        ++level_ptr;
        ++index;
    }
    fprintf(REDIS_INSPECT_STD_OUT, "\n\n");

    node = zskiplist_ptr->tail;
    while (node != zskiplist_ptr->header && node != NULL)
    {
        fprintf(REDIS_INSPECT_STD_OUT, "node: %p\n", (void *)node);
        fprintf(REDIS_INSPECT_STD_OUT, "node->ele: ");
        inspect_normal_string(node->ele);
        fprintf(REDIS_INSPECT_STD_OUT, "node->score: %lf\n", node->score);
        fprintf(REDIS_INSPECT_STD_OUT, "node->backward: %p\n", (void *)node->backward);
        level_ptr = &node->level[0];
        index = 0;
        while (index < zskiplist_ptr->level)
        {
            fprintf(REDIS_INSPECT_STD_OUT, "node->level[%d].forward: %p\n", index, (void *)node->level[index].forward);
            fprintf(REDIS_INSPECT_STD_OUT, "node->level[%d].span: %lu\n", index, node->level[index].span);
            ++level_ptr;
            ++index;
        }
        /*
        fprintf(REDIS_INSPECT_STD_OUT, "node->level[%lu].forward: %p\n", index, (void *)node->level[index].forward);
        fprintf(REDIS_INSPECT_STD_OUT, "node->level[%lu].span: %lu\n", index, node->level[index].span);
         */
        if (node != NULL)
            node = node->backward;
    }
}

void _inspect_zset(zset *zset_ptr)
{
    fprintf(REDIS_INSPECT_STD_OUT, "zset_ptr->dict: %p, zset_ptr->zsl: %p\n", (void *)zset_ptr->dict, (void *)zset_ptr->zsl);
    inspect_dict(zset_ptr->dict);
    inspect_zskiplist(zset_ptr->zsl);
}

void inspect_zset(robj *o)
{
    if (o->encoding == OBJ_ENCODING_ZIPLIST)
    {
        inspect_ziplist(o->ptr);
    }
    else if (o->encoding == OBJ_ENCODING_SKIPLIST)
        _inspect_zset((zset *)o->ptr);
    else
        fprintf(REDIS_INSPECT_STD_OUT, "unknown encoding: %d\n", o->encoding);
}
