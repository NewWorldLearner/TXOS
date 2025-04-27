#include "../include/bitmap.h"
#include "../include/print_kernel.h"
#include "../include/debug.h"

int bitmap_test(struct bitmap *bitmap, uint64_t bit_index)
{
    ASSERT(bit_index < bitmap->length * 8);
    int i = bit_index >> 6;
    int j = bit_index % 64;
    return bitmap->bits[i] & (1UL << j);
}

int bitmap_scan(struct bitmap *bitmap, uint64_t count)
{

    int total_bits = bitmap->length * 8;
    int total_words = bitmap->length / 8;
    if (count == 0 || count > total_bits)
    {
        return -1;
    }

    uint32_t word_idx = 0;
    // 每次比较8个字节
    while (word_idx < total_words && bitmap->bits[word_idx] == 0xffffffffffffffff)
    {
        word_idx++;
    }
    if (word_idx == total_words)
    {
        printf("bitmap scan error\n");
        return -1;
    }
    int bit_index = 0;

    // 找到第i个unsigned long中的空闲位
    while (bitmap->bits[word_idx] & (1UL << bit_index))
    {
        bit_index++;
    }

    int bitmap_free_index = word_idx * 64 + bit_index;
    if (count == 1)
    {
        return bitmap_free_index;
    }

    // 比较接下来的n个位是否都是空闲位
    int next_bit = bitmap_free_index + 1;
    int free_count = 1;
    while (free_count <= count && next_bit < total_bits)
    {
        if (bitmap_test(bitmap, next_bit))
        {
            free_count = 0;
        }
        free_count++;
        next_bit++;
    }
    // 接下来还需要判断获取到的位是否超过了位界限
    if (next_bit > bitmap->length * 8)
    {
        return -1;
    }
    return next_bit - count;
}

void bitmap_set(struct bitmap *bitmap, uint64_t bit_index, int8_t value)
{
    int i = bit_index >> 6;
    int j = bit_index % 64;
    if (value)
    {
        bitmap->bits[i] |= (1UL << j);
    }
    else
    {
        bitmap->bits[i] &= ~(1UL << j);
    }
}