//
// Created by zpoint on 2019/8/25.
//

#pragma once
#include "util.h"
#include "../src/hyperloglog.h"

void inspect_dense_hyperloglog(struct hllhdr *hdr)
{

}

void inspect_sparse_hyperloglog(struct hllhdr *hdr)
{

}

void inspect_hyperloglog(robj *o)
{
    struct hllhdr *hdr = o->ptr;
    uint64_t card;

    fprintf(REDIS_INSPECT_STD_OUT, "magic: %hhx(%c) %hhx(%c) %hhx(%c) %hhx(%c)\n", hdr->magic[0], hdr->magic[0], hdr->magic[1], hdr->magic[1], hdr->magic[2], hdr->magic[2], hdr->magic[3], hdr->magic[3]);
    if (hdr->encoding == HLL_DENSE)
    {
        fprintf(REDIS_INSPECT_STD_OUT, "encoding: %hhx(HLL_DENSE)\n", hdr->encoding);
    }
    else
    {
        fprintf(REDIS_INSPECT_STD_OUT, "encoding: %hhx(HLL_SPARSE)\n", hdr->encoding);
    }
    if ((((hdr)->card[7]) & (1 << 7)) == 1)
    {
        fprintf(REDIS_INSPECT_STD_OUT, "HLL_VALID_CACHE: value: ");
    }
    else
    {
        fprintf(REDIS_INSPECT_STD_OUT, "HLL_INVALIDATE_CACHE: value: ");
    }
    card = (uint64_t)hdr->card[0];
    card |= (uint64_t)hdr->card[1] << 8;
    card |= (uint64_t)hdr->card[2] << 16;
    card |= (uint64_t)hdr->card[3] << 24;
    card |= (uint64_t)hdr->card[4] << 32;
    card |= (uint64_t)hdr->card[5] << 40;
    card |= (uint64_t)hdr->card[6] << 48;
    card |= (uint64_t)hdr->card[7] << 56;
    fprintf(REDIS_INSPECT_STD_OUT, "%llu\n", card);
    pr_binary_64(card);
    fprintf(REDIS_INSPECT_STD_OUT, "\n");
    if (hdr->encoding == HLL_DENSE)
        inspect_dense_hyperloglog(hdr);
    else
        inspect_sparse_hyperloglog(hdr);
}
