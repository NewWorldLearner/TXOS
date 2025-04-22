
#ifndef _LIST_H_
#define _LIST_H_

#include "stdint.h"

#define container_of(ptr, type, member)                                     \
	({                                                                      \
		typeof(((type *)0)->member) *p = (ptr);                             \
		(type *)((unsigned long)p - (unsigned long)&(((type *)0)->member)); \
	})

struct List
{
	struct List * prev;
	struct List * next;
};

void list_init(struct List *list);
void list_insert_after(struct List *entry, struct List *new_node);
void list_insert_before(struct List *entry, struct List *new_node);
void list_delete(struct List *entry);
long list_is_empty(struct List *entry);
struct List *list_prev(struct List *entry);
struct List *list_next(struct List *entry);

#endif