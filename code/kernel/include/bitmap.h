#ifndef _BITMAP_H_
#define _BITMAP_H_

#include "stdint.h"

struct bitmap
{
    uint64_t *bits;       // 位图的起始地址
    uint64_t length;        // 位图所占的字节
};

int bitmap_test(struct bitmap *bitmap, uint64_t bit_index);
int bitmap_scan(struct bitmap *bitmap, uint64_t count);
void bitmap_set(struct bitmap *bitmap, uint64_t bit_index, int8_t value);

#endif