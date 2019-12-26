//
// Created by zpoint on 2019-07-24.
//
#pragma once
#include "util.h"
#include "../src/dict.h"

void inspect_ziplist(char *ptr)
{
    char *ptr_entry;
    uint32_t *tmp_ptr, prev_length, prev_length_width;
    uint32_t *ptr_32 = (uint32_t *)ptr;
    uint16_t *ptr_16 = (uint16_t *)ptr;
    uint16_t zip_list_length;
    int is_string = 0;
    uint32_t field_length = 0, encoding_length = 0;
    unsigned char left_four, right_four;
    int8_t int8_val;
    uint8_t uint8_val;
    int16_t int16_val;
    int32_t int32_val;
    uint32_t uint32_val;
    int64_t int64_val;

    fprintf(REDIS_INSPECT_STD_OUT, "ziplist header\n");
    fprintf(REDIS_INSPECT_STD_OUT, "first uint32_t(zlbytes): ");
    pr_binary_32(*ptr_32);
    ++ptr_32;
    fprintf(REDIS_INSPECT_STD_OUT, "\nsecond uint32_t(zltail): ");
    pr_binary_32(*ptr_32);
    ptr_16 = (uint16_t *)++ptr_32;
    fprintf(REDIS_INSPECT_STD_OUT, "\nuint16_t(zllen): ");
    zip_list_length = *ptr_16;
    pr_binary_16(*ptr_16);
    fprintf(REDIS_INSPECT_STD_OUT, "\n");
    ptr_entry = (char *)++ptr_16;
    for (size_t i = 0; i < zip_list_length; ++i)
    {
        fprintf(REDIS_INSPECT_STD_OUT, "entry index: %zu, value in ptr_entry: %p\n", i, (void *)ptr_entry);
        if (ptr_entry[0] == (char) 255)
        {
            fprintf(REDIS_INSPECT_STD_OUT, "zlend: hex: %hhx, binary: ", ptr_entry[0]);
            pr_binary(ptr_entry[0]);
            fprintf(REDIS_INSPECT_STD_OUT, "\n");
            continue;
        } else if (ptr_entry[0] == (char) 254)
        {
            // previous length takes the rest 4 bytes
            fprintf(REDIS_INSPECT_STD_OUT, "prevlen(5 bytes), the 1th byte, hex: %hhx, binary: ", ptr_entry[0]);
            pr_binary(ptr_entry[0]);
            tmp_ptr = (uint32_t *)++ptr_entry;
            fprintf(REDIS_INSPECT_STD_OUT, "\nthe rest 4 bytes in unsigned int, hex: %u, binary: ", *tmp_ptr);
            pr_binary_32(*tmp_ptr);
            prev_length = *tmp_ptr;
            prev_length_width = 5;
            ptr_entry = (char *) ++tmp_ptr;
            fprintf(REDIS_INSPECT_STD_OUT, "\n");
        }
        else
        {
            // previous length is stored in this byte
            fprintf(REDIS_INSPECT_STD_OUT, "prevlen(1 byte): hex: %hhx, binary: ", ptr_entry[0]);
            pr_binary(ptr_entry[0]);
            fprintf(REDIS_INSPECT_STD_OUT, "\n");
            prev_length = *ptr_entry;
            prev_length_width = 1;
            ++ptr_entry;
        }
        // ptr_entry is the <encoding> now
        is_string = 0;
        field_length = 0;
        encoding_length = 0;

        fprintf(REDIS_INSPECT_STD_OUT, "ziplist_encoding(1 byte): hex: %hhx, binary: ", ptr_entry[0]);
        pr_binary(ptr_entry[0]);
        fprintf(REDIS_INSPECT_STD_OUT, "\n");
        switch (((*ptr_entry) >> 6) & (unsigned char) 3) {
            case 0:
                /*
                 *      |00pppppp| - 1 byte
                 *      String value with length less than or equal to 63 bytes (6 bits).
                 *      "pppppp" represents the unsigned 6 bit length.
                 */
                is_string = 1;
                encoding_length = 1;
                field_length = *ptr_entry & ~((unsigned char) 3 << 6);
                ++ptr_entry;
                break;
            case 1:
                /*
                 *      |01pppppp|qqqqqqqq| - 2 bytes
                 *      String value with length less than or equal to 16383 bytes (14 bits).
                 *      IMPORTANT: The 14 bit number is stored in big endian.
                 */
                is_string = 1;
                encoding_length = 2;
                field_length = *ptr_entry & ~((unsigned char) 3 << 6);
                field_length <<= 8;
                field_length += *++ptr_entry;
                ++ptr_entry;
                break;
            case 2:
                /*
                 *    |10000000|qqqqqqqq|rrrrrrrr|ssssssss|tttttttt| - 5 bytes
                 *    String value with length greater than or equal to 16384 bytes.
                 *    Only the 4 bytes following the first byte represents the length
                 *    up to 32^2-1. The 6 lower bits of the first byte are not used and
                 *    are set to zero.
                 *    IMPORTANT: The 32 bit number is stored in big endian.
                 */
                is_string = 1;
                encoding_length = 5;
                for (size_t i = 0; i < 4; ++i) {
                    field_length <<= 8;
                    field_length += *++ptr_entry;
                }
                ++ptr_entry;
                break;
            case 3:
                /*
                 * |11000000| - 3 bytes
                 *      Integer encoded as int16_t (2 bytes).
                 * |11010000| - 5 bytes
                 *      Integer encoded as int32_t (4 bytes).
                 * |11100000| - 9 bytes
                 *      Integer encoded as int64_t (8 bytes).
                 * |11110000| - 4 bytes
                 *      Integer encoded as 24 bit signed (3 bytes).
                 * |11111110| - 2 bytes
                 *      Integer encoded as 8 bit signed (1 byte).
                 * |1111xxxx| - (with xxxx between 0000 and 1101) immediate 4 bit integer.
                 *      Unsigned integer from 0 to 12. The encoded value is actually from
                 *      1 to 13 because 0000 and 1111 can not be used, so 1 should be
                 *      subtracted from the encoded 4 bit value to obtain the right value.
                 * |11111111| - End of ziplist special entry.
                 */
                encoding_length = 1;

                left_four = ((unsigned char)(*ptr_entry)) >> 4;
                right_four = ((unsigned char)(*ptr_entry)) & 0x0f;
                fprintf(REDIS_INSPECT_STD_OUT, "following is integer value\n");
                switch (left_four) {
                    case 12:
                        // 1100
                        ++ptr_entry;
                        int16_val = *(int16_t *) ptr_entry;
                        fprintf(REDIS_INSPECT_STD_OUT, "2 bytes(int16_t), decimal: %d, binary: ", int16_val);
                        pr_binary_16(int16_val);
                        fprintf(REDIS_INSPECT_STD_OUT, "\n");
                        ptr_entry = (char *) ((int16_t *)ptr_entry + 1);
                        field_length = 2;
                        break;
                    case 13:
                        // 1101
                        ++ptr_entry;
                        int32_val = *(int32_t *) ptr_entry;
                        fprintf(REDIS_INSPECT_STD_OUT, "4 bytes(int32_t), decimal: %d, binary: ", int32_val);
                        pr_binary_32(int32_val);
                        fprintf(REDIS_INSPECT_STD_OUT, "\n");
                        ptr_entry = (char *) ((int32_t *)ptr_entry + 1);
                        field_length = 4;
                        break;
                    case 14:
                        // 1110
                        ++ptr_entry;
                        int64_val = *(int64_t *) ptr_entry;
                        fprintf(REDIS_INSPECT_STD_OUT, "8 bytes(int64_t), decimal: %lld, binary: ", int64_val);
                        pr_binary_32(int64_val);
                        fprintf(REDIS_INSPECT_STD_OUT, "\n");
                        ++int64_val;
                        ptr_entry = (char *) ((int64_t *)int64_val + 1);
                        field_length = 8;
                        break;
                    case 15:
                        // 1111
                        if (right_four == 0) {
                            ++ptr_entry;
                            uint32_val = 0;
                            uint8_val = 0;
                            for (size_t i = 1; i < 4; ++i)
                            {
                                /* little endian */
                                uint8_val = *(uint8_t *)ptr_entry;
                                uint32_val += (uint32_t)uint8_val << (i * 8);
                            }
                            int32_val = (int32_t)uint32_val >> 8;
                            fprintf(REDIS_INSPECT_STD_OUT, "encoded as 24bit signed, following int32_t adds as the little endian order, decimal: %d, binary: ", int32_val);
                            pr_binary_32(int32_val);
                            fprintf(REDIS_INSPECT_STD_OUT, "\n");
                            field_length = 3;
                        }
                        else if (right_four == 14)
                        {
                            int8_val = *(++ptr_entry);
                            fprintf(REDIS_INSPECT_STD_OUT,
                                    "encoded as 8bit signed, following int8_t val, decimal: %d, binary: ",
                                    int8_val);
                            pr_binary(int8_val);
                            fprintf(REDIS_INSPECT_STD_OUT, "\n");
                            ++ptr_entry;
                            field_length = 1;
                        }
                        else if (right_four == 15)
                        {
                            fprintf(REDIS_INSPECT_STD_OUT, "End of ziplist special entry\n");
                            ++ptr_entry;
                        }
                        else
                        {
                            fprintf(REDIS_INSPECT_STD_OUT,
                                    "encoded as 4bit signed, following int8_t mask the first 4 bit, decimal: %d, binary: ",
                                    right_four);
                            pr_binary(right_four);
                            fprintf(REDIS_INSPECT_STD_OUT, "\n");
                            ++ptr_entry;
                            field_length = 0;
                        }
                        break;
                    default:
                        fprintf(REDIS_INSPECT_STD_OUT, "unknown left_four, left_four: %hhx\n", left_four);
                }
                break;
            default:
                fprintf(REDIS_INSPECT_STD_OUT, "unknown encoding in zip list: %hhx\n", ptr_entry[0]);
        }

        fprintf(REDIS_INSPECT_STD_OUT, "extracted field_length: %u, entry length(prev_length(%u) + "
                                       "encoding_length(%u) + field_length(%u)): %u\n",
                field_length, prev_length_width, encoding_length, field_length,
                prev_length_width + encoding_length + field_length);
        if (is_string)
        {
            for (size_t i = 0; i < field_length; ++i, ++ptr_entry)
            {
                fprintf(REDIS_INSPECT_STD_OUT, "field index: %zu, hex: %hhx, chr: %c, binary: ", i, *ptr_entry, *ptr_entry);
                pr_binary(*ptr_entry);
                fprintf(REDIS_INSPECT_STD_OUT, "\n");
            }
        }
        fprintf(REDIS_INSPECT_STD_OUT, "\n");

    }

    if (zip_list_length == UINT16_MAX)
    {
        fprintf(REDIS_INSPECT_STD_OUT, "length of the zip list is longer than UINT16_MAX\n");
    }
    fprintf(REDIS_INSPECT_STD_OUT, "after the for loop, it should be zlend(uint8_t), ptr_entry_value hex: %hhx, binary: ", *ptr_entry);
    pr_binary(*ptr_entry);
    fprintf(REDIS_INSPECT_STD_OUT, "\n\n\n");
}


void inspect_ht(dictht *ptr2ht)
{
    dictEntry **table = ptr2ht->table;
    dictEntry *entry, *next;
    fprintf(REDIS_INSPECT_STD_OUT, "size: %lu, sizemask: %lu, used: %lu\n", ptr2ht->size, ptr2ht->sizemask, ptr2ht->used);
    for (unsigned long i = 0; i < ptr2ht->size; ++i)
    {
        entry = table[i];
        fprintf(REDIS_INSPECT_STD_OUT, "table[%ld]: %p, addr of table[%ld]: %p\n", i, (void *)entry, i, (void *)&table[i]);
        if (entry)
        {
            next = entry;
            while (next != NULL)
            {
                fprintf(REDIS_INSPECT_STD_OUT, "entry->key: %p, entry->val: %p, entry->next: %p\n", next->key, next->v.val, (void *)next->next);
                inspect_normal_string(next->key);
                inspect_normal_string(next->v.val);
                next = next->next;
            }
            fprintf(REDIS_INSPECT_STD_OUT, "\n\n");
        }
    }
}

void inspect_dict(dict *ptr2dict)
{
    fprintf(REDIS_INSPECT_STD_OUT, "type value: %p, type addr: %p, privdata value: %p, privdata addr: %p, ht[0]: %p, "
                                   "ht[1]: %p, long rehashidx, value: %ld, addr: %p, unsigned long iterators, "
                                   "value: %lu, addr: %p\n",
                                   (void *)ptr2dict->type, (void *)&ptr2dict->type, ptr2dict->privdata,
                                   (void *)&ptr2dict->privdata, (void *)&ptr2dict->ht[0], (void *)&ptr2dict->ht[1],
                                   ptr2dict->rehashidx, (void *)&ptr2dict->rehashidx, ptr2dict->iterators, (void *)&ptr2dict->iterators);
    fprintf(REDIS_INSPECT_STD_OUT, "inspecting ht[0]\n");
    inspect_ht(&ptr2dict->ht[0]);
    fprintf(REDIS_INSPECT_STD_OUT, "inspecting ht[1]\n");
    inspect_ht(&ptr2dict->ht[1]);
}

void inspect_hash(robj *o)
{
    if (o->encoding == OBJ_ENCODING_ZIPLIST)
        inspect_ziplist((char *)o->ptr);
    else if (o->encoding == OBJ_ENCODING_HT)
        inspect_dict((dict *)o->ptr);
    else
        fprintf(REDIS_INSPECT_STD_OUT, "unknown encoding: %d\n", o->encoding);
}
