//
// Created by zpoint on 2019/11/30.
//

#pragma once
#include "util.h"
#include "../src/rax.h"

int rax_padding(raxNode *ptr_node)
{
    size_t r = (sizeof(void *) - ((ptr_node->size + 4) % sizeof(void *))) & (sizeof(void*) - 1);
    return r;
}

void inspect_rax_node(raxNode *ptr_node, void (func)(void *), void (pr_node_func)(raxNode *ptr_node))
{
    void *aux = NULL;
    raxNode** ptr2next_node = NULL;
    fprintf(REDIS_INSPECT_STD_OUT, "\ninspect_rax_node begin, address of ptr_node: %p\n", (void *)ptr_node);
    fprintf(REDIS_INSPECT_STD_OUT, "iskey: %u\n", ptr_node->iskey);
    fprintf(REDIS_INSPECT_STD_OUT, "isnull: %u\n", ptr_node->isnull);
    fprintf(REDIS_INSPECT_STD_OUT, "iscompr: %u\n", ptr_node->iscompr);
    fprintf(REDIS_INSPECT_STD_OUT, "size: %u\n", ptr_node->size);
    pr_node_func(ptr_node);
    fprintf(REDIS_INSPECT_STD_OUT, "\n");
    if (ptr_node->iscompr)
    {
        ptr2next_node = (raxNode**)&(ptr_node->data[ptr_node->size + rax_padding(ptr_node)]);
        fprintf(REDIS_INSPECT_STD_OUT, "ptr2next_node address: %p, value: %p\n", (void *)ptr2next_node, (void *)*ptr2next_node);
        if (*ptr2next_node)
            inspect_rax_node(*ptr2next_node, func, pr_node_func);
        aux = ptr2next_node;
    }
    else
    {
        fprintf(REDIS_INSPECT_STD_OUT, "char address: \n");
        ptr2next_node = (raxNode**)&(ptr_node->data[ptr_node->size + rax_padding(ptr_node)]);
        aux = ptr2next_node;
        for (size_t i = 0; i < ptr_node->size; ++i)
        {
            ptr2next_node += i;
            fprintf(REDIS_INSPECT_STD_OUT, "address: %p, value: %p, i: %lu\n", (void *)ptr2next_node, (void *)*ptr2next_node, i);
            if (*ptr2next_node)
                inspect_rax_node(*ptr2next_node, func, pr_node_func);
            fprintf(REDIS_INSPECT_STD_OUT, "\n");
            aux = ptr2next_node + 1;
        }
        fprintf(REDIS_INSPECT_STD_OUT, "\n");
    }
    if (ptr_node->iskey && !(ptr_node->isnull))
    {
        fprintf(REDIS_INSPECT_STD_OUT, "\ninspect_rax_node end, aux: %p, *aux: %p, address of ptr_node: %p\n", aux, *(void **) aux, (void *) ptr_node);
        // only for stream inspecting
        func(*(void **) aux);
    }
    else
        fprintf(REDIS_INSPECT_STD_OUT, "\ninspect_rax_node end, address of ptr_node: %p\n", (void *)ptr_node);
}

void inspect_rax(rax *ptr_rax, void (func)(void *), void (pr_node_func)(raxNode *ptr_node))
{
    fprintf(REDIS_INSPECT_STD_OUT, "rax ptr_rax->ptr: %p\n", (void *)ptr_rax);
    if (ptr_rax)
    {
        fprintf(REDIS_INSPECT_STD_OUT, "ptr_rax->head: %p, addr: %p\n", (void *)ptr_rax->head, (void *)&ptr_rax->head);
        fprintf(REDIS_INSPECT_STD_OUT, "ptr_rax->numele: %llu, addr: %p\n", ptr_rax->numele, (void *)&ptr_rax->numele);
        fprintf(REDIS_INSPECT_STD_OUT, "ptr_rax->numnodes: %llu, addr: %p\n", ptr_rax->numnodes, (void *)&ptr_rax->numnodes);
        inspect_rax_node(ptr_rax->head, func, pr_node_func);
    }
}

