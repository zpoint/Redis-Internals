//
// Created by zpoint on 2019/11/30.
//

#pragma once
#include "util.h"
#include "../src/listpack.h"

size_t encoding_size(char chr)
{
    if ((chr & 128) == 0)
    {
        // LP_ENCODING_7BIT_UINT
        return 1;
    }
    return 0;
}

void inspect_listpack(void* lp)
{
    // #define LP_HDR_SIZE 6       /* 32 bit total len + 16 bit number of elements. */
    size_t back_length;
    size_t back_val;
    size_t back_shift;
    size_t curr_size;
    unsigned char *ptr_val = (unsigned char *)lp;
    uint16_t num_ele = 0;
    uint32_t total_bytes = 0, current_total_bytes = 0;
    uint64_t negstart = 0, uval = 0, negmax = 0;
    int64_t val;

    fprintf(REDIS_INSPECT_STD_OUT, "32 bit total len: ");
    total_bytes = *(unsigned int *)ptr_val;
    pr_binary_32(total_bytes);
    ptr_val += 4;
    current_total_bytes += 4;
    num_ele = *(uint16_t *)ptr_val;
    fprintf(REDIS_INSPECT_STD_OUT, "\n16 bit number of elements: ");
    pr_binary_16(num_ele);
    fprintf(REDIS_INSPECT_STD_OUT, "\n");
    ptr_val += 2;
    current_total_bytes += 2;

    for (size_t i = 0; i < num_ele; ++i)
    {
        back_length = 0;
        if (((*ptr_val) & 0x80) == 0)
        {
            /* 7BIT_UINT */
            // #define LP_ENCODING_7BIT_UINT 0         // 0000 0000
            // #define LP_ENCODING_7BIT_UINT_MASK 0x80 // 1000 0000
            fprintf(REDIS_INSPECT_STD_OUT, "7BIT_UINT, value: %d, addr: %p, binary: ", (0x7f & (unsigned int)(*ptr_val)), (void *)ptr_val);
            pr_binary(*ptr_val);
            fprintf(REDIS_INSPECT_STD_OUT, "(0x%hhx)\n", *ptr_val);
            ++ptr_val;
            current_total_bytes += 1;
            back_length = 1;
        }
        else if (((*ptr_val) & 0xE0) == 0xC0)
        {
            /* 13BIT_INT */
            // #define LP_ENCODING_13BIT_INT 0xC0      // 1100 0000
            // #define LP_ENCODING_13BIT_INT_MASK 0xE0 // 1110 0000
            uval = ((31 & ptr_val[0]) << 8) | ptr_val[1];
            negstart = (uint64_t)1<<12;
            negmax = (1 << 13) - 1;
            // copy and pasted
            if (uval >= negstart) {
                /* This three steps conversion should avoid undefined behaviors
                 * in the unsigned -> signed conversion. */
                uval = negmax-uval;
                val = uval;
                val = -val-1;
            } else {
                val = uval;
            }

            fprintf(REDIS_INSPECT_STD_OUT, "13BIT_INT, value: %lld, addr: %p, binary: ", val, (void *)ptr_val);
            pr_binary(*ptr_val);
            fprintf(REDIS_INSPECT_STD_OUT, "(0x%hhx) ", *ptr_val);
            pr_binary(*(ptr_val+1));
            fprintf(REDIS_INSPECT_STD_OUT, "(0x%hhx)\n", *(ptr_val+1));
            ptr_val += 2;
            current_total_bytes += 2;
            back_length = 1;

        }
        else if (((*ptr_val) & 0xFF) == 0xF1)
        {
            /* 16BIT_INT */
            // #define LP_ENCODING_16BIT_INT 0xF1      // 1111 0001
            // #define LP_ENCODING_16BIT_INT_MASK 0xFF // 1111 1111
            uval = (ptr_val[2] << 8) | ptr_val[1];
            negstart = (uint64_t)1<<15;
            negmax = (1 << 16) - 1;
            // copy and pasted
            if (uval >= negstart) {
                /* This three steps conversion should avoid undefined behaviors
                 * in the unsigned -> signed conversion. */
                uval = negmax-uval;
                val = uval;
                val = -val-1;
            } else {
                val = uval;
            }

            fprintf(REDIS_INSPECT_STD_OUT, "16BIT_INT, value: %lld, addr: %p, binary: ", val, (void *)ptr_val);
            pr_binary(*ptr_val);
            fprintf(REDIS_INSPECT_STD_OUT, "(0x%hhx) ", *ptr_val);
            pr_binary(*(ptr_val+1));
            fprintf(REDIS_INSPECT_STD_OUT, "(0x%hhx) ", *(ptr_val+1));
            pr_binary(*(ptr_val+2));
            fprintf(REDIS_INSPECT_STD_OUT, "(0x%hhx)\n", *(ptr_val+2));
            ptr_val += 3;
            current_total_bytes += 3;
            back_length = 1;
        }
        else if (((*ptr_val) & 0xFF) == 0xF2)
        {
            /* 24BIT_INT */
            // #define LP_ENCODING_24BIT_INT 0xF2      // 1111 0010
            // #define LP_ENCODING_24BIT_INT_MASK 0xFF // 1111 1111
            uval = (ptr_val[3] << 16) | (ptr_val[2] << 8) | ptr_val[1];
            negstart = (uint64_t)1<<23;
            negmax = (1 << 24) - 1;
            // copy and pasted
            if (uval >= negstart) {
                /* This three steps conversion should avoid undefined behaviors
                 * in the unsigned -> signed conversion. */
                uval = negmax-uval;
                val = uval;
                val = -val-1;
            } else {
                val = uval;
            }

            fprintf(REDIS_INSPECT_STD_OUT, "24BIT_INT, value: %lld, addr: %p, binary: ", val, (void *)ptr_val);
            pr_binary(*ptr_val);
            fprintf(REDIS_INSPECT_STD_OUT, "(0x%hhx) ", *ptr_val);
            pr_binary(*(ptr_val+1));
            fprintf(REDIS_INSPECT_STD_OUT, "(0x%hhx) ", *(ptr_val+1));
            pr_binary(*(ptr_val+2));
            fprintf(REDIS_INSPECT_STD_OUT, "(0x%hhx) ", *(ptr_val+2));
            pr_binary(*(ptr_val+3));
            fprintf(REDIS_INSPECT_STD_OUT, "(0x%hhx)\n", *(ptr_val+3));
            ptr_val += 4;
            current_total_bytes += 4;
            back_length = 1;

        }
        else if (((*ptr_val) & 0xFF) == 0xF3)
        {
            /* 32BIT_INT */
            // #define LP_ENCODING_32BIT_INT 0xF3      // 1111 0011
            // #define LP_ENCODING_32BIT_INT_MASK 0xFF // 1111 1111
            uval = (ptr_val[4] << 24) | (ptr_val[3] << 16) | (ptr_val[2] << 8) | ptr_val[1];
            negstart = (uint64_t)1<<31;
            negmax = ((uint64_t)1 << 32) - 1;
            // copy and pasted
            if (uval >= negstart) {
                /* This three steps conversion should avoid undefined behaviors
                 * in the unsigned -> signed conversion. */
                uval = negmax-uval;
                val = uval;
                val = -val-1;
            } else {
                val = uval;
            }

            fprintf(REDIS_INSPECT_STD_OUT, "32BIT_INT, value: %lld, addr: %p, binary: ", val, (void *)ptr_val);
            pr_binary(*ptr_val);
            fprintf(REDIS_INSPECT_STD_OUT, "(0x%hhx) ", *ptr_val);
            pr_binary(*(ptr_val+1));
            fprintf(REDIS_INSPECT_STD_OUT, "(0x%hhx) ", *(ptr_val+1));
            pr_binary(*(ptr_val+2));
            fprintf(REDIS_INSPECT_STD_OUT, "(0x%hhx) ", *(ptr_val+2));
            pr_binary(*(ptr_val+3));
            fprintf(REDIS_INSPECT_STD_OUT, "(0x%hhx) ", *(ptr_val+3));
            pr_binary(*(ptr_val+4));
            fprintf(REDIS_INSPECT_STD_OUT, "(0x%hhx)\n", *(ptr_val+4));
            ptr_val += 5;
            current_total_bytes += 5;
            back_length = 1;
        }
        else if (((*ptr_val) & 0xC0) == 0x80)
        {
            /* 6BIT_STR */
            // #define LP_ENCODING_6BIT_STR 0x80      // 1000 0000
            // #define LP_ENCODING_6BIT_STR_MASK 0xC0 // 1100 0000
            fprintf(REDIS_INSPECT_STD_OUT, "6BIT_STR, value: %d, addr: %p, binary: ", (63 & (unsigned int)(*ptr_val)), (void *)ptr_val);
            pr_binary(*ptr_val);
            fprintf(REDIS_INSPECT_STD_OUT, "(0x%hhx)\n", *ptr_val);
            curr_size = (63 & (unsigned int)(*ptr_val));
            current_total_bytes += curr_size + 1;
            back_length = 1;
            ++ptr_val;
            for (size_t j = 0; j < curr_size; ++j)
            {
                fprintf(REDIS_INSPECT_STD_OUT, "%c(%hhx) ", *ptr_val, *ptr_val);
                ++ptr_val;
            }
            fprintf(REDIS_INSPECT_STD_OUT, "\n");
        }
        else if (((*ptr_val) & 0xF0) == 0xE0)
        {
            /* 12BIT_STR */
            // #define LP_ENCODING_12BIT_STR 0xE0       // 1110 0000
            // #define LP_ENCODING_12BIT_STR_MASK 0xF0  // 1111 0000
            curr_size = ((15 & ptr_val[0]) << 8) | ptr_val[1];
            fprintf(REDIS_INSPECT_STD_OUT, "12BIT_STR, value: %zu, addr: %p, binary: ", curr_size, (void *)ptr_val);
            pr_binary(*ptr_val);
            fprintf(REDIS_INSPECT_STD_OUT, "(0x%hhx) ", *ptr_val);
            pr_binary(*(ptr_val+1));
            fprintf(REDIS_INSPECT_STD_OUT, "(0x%hhx)\n", *(ptr_val+1));
            current_total_bytes += curr_size + 2;
            if (curr_size > 127)
                back_length = 2;
            else
                back_length = 1;
            ptr_val += 2;
            for (size_t j = 0; j < curr_size; ++j)
            {
                fprintf(REDIS_INSPECT_STD_OUT, "%c(%hhx) ", *ptr_val, *ptr_val);
                ++ptr_val;
            }
            fprintf(REDIS_INSPECT_STD_OUT, "(0x%hhx)\n", *ptr_val);
        }
        else
        {
            fprintf(REDIS_INSPECT_STD_OUT, "unknown type, going to break, binary: ");
            pr_binary(*ptr_val);
            fprintf(REDIS_INSPECT_STD_OUT, "(0x%hhx)\n", *ptr_val);
            break;
        }
        back_val = 0;
        back_shift = 0;
        /* back length */
        do
        {

            /* big endian, 7 bit per bytes */
            fprintf(REDIS_INSPECT_STD_OUT, "proceeding back size, addr: %p, binary: ", (void *)ptr_val);
            pr_binary(*ptr_val);
            fprintf(REDIS_INSPECT_STD_OUT, "(0x%hhx)\n", *ptr_val);

            back_val |= (*ptr_val & 127) << back_shift;
            back_shift += 7;
            back_length--;
            ++ptr_val;
        } while (back_length);
        fprintf(REDIS_INSPECT_STD_OUT, "back_size: %zu\n", back_val);
    }
    fprintf(REDIS_INSPECT_STD_OUT, "it should be LP_EOF, binary: ");
    pr_binary(*ptr_val);
    fprintf(REDIS_INSPECT_STD_OUT, "\n");
}
