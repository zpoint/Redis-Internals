//
// Created by zpoint on 2019-08-02.
//

#pragma once
#include "util.h"
#include "inspect_dict.c"
#include "../src/quicklist.h"

void inspect_list(robj *o)
{
    quicklist *lst = (quicklist *)o->ptr;
    quicklistNode *curr = lst->head;
    size_t i = 0;
    fprintf(REDIS_INSPECT_STD_OUT, "head: %p\n", (void *)lst->head);
    fprintf(REDIS_INSPECT_STD_OUT, "tail: %p\n", (void *)lst->tail);
    fprintf(REDIS_INSPECT_STD_OUT, "count: %lu\n", lst->count);
    fprintf(REDIS_INSPECT_STD_OUT, "len: %lu\n", lst->len);
    fprintf(REDIS_INSPECT_STD_OUT, "fill: %i\n", lst->fill);
    fprintf(REDIS_INSPECT_STD_OUT, "compress: %u\n", lst->compress);
    do
        {
            fprintf(REDIS_INSPECT_STD_OUT, "%zu th: addr: %p\n", i, (void *)curr);
            fprintf(REDIS_INSPECT_STD_OUT, "prev: %p, next: %p, zl: %p, sz val: %u, sz addr: %p, count: %u, encoding: %u, "
                                           "container: %u, recompress: %u, attempted_compress: %u, "
                                           "extra: %u\n", (void *)curr->prev, (void *)curr->next, (void *)curr->zl,
                                           curr->sz, (void *)&curr->sz, curr->count, curr->encoding, curr->container,
                                           curr->recompress, curr->attempted_compress, curr->extra);
            if (curr->encoding == QUICKLIST_NODE_ENCODING_RAW)
                inspect_ziplist((char *)curr->zl);
            else
            {
                quicklistLZF *lzf = (quicklistLZF *)curr->zl;
                fprintf(REDIS_INSPECT_STD_OUT, "encoding: QUICKLIST_NODE_ENCODING_LZF\n");
                fprintf(REDIS_INSPECT_STD_OUT, "sz: %u\n", lzf->sz);
                for (size_t i = 0; i < lzf->sz; ++i)
                {
                    fprintf(REDIS_INSPECT_STD_OUT, "0x%0hhx ", lzf->compressed[i]);
                }
                fprintf(REDIS_INSPECT_STD_OUT, "\n");
            }
            curr = curr->next;
            ++i;

        }
    while (curr != NULL);
}
