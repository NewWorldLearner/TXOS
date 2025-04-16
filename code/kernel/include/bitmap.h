#ifndef _BITMAP_H_
#define _BITMAP_H_

#include "stdint.h"

int bitmap_test(uint64_t *bitmap, uint64_t bit_index);
int bitmap_scan(uint64_t *bitmap, uint32_t bitmap_length, uint32_t count);
void bitmap_set(uint64_t *bitmap, uint64_t bit_index, int8_t value);

#endif