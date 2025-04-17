#include "../include/list.h"
#include "../include/debug.h"


#define NULL (void*)0

void list_init(struct List *list)
{
	ASSERT(list != NULL);
	list->prev = list;
	list->next = list;
}

// 插入一个结点到enter后面
void list_insert_after(struct List * entry,struct List * new_node)
{
	ASSERT(entry != NULL);
	ASSERT(new_node != NULL);

	new_node->next = entry->next;
	new_node->prev = entry;
	new_node->next->prev = new_node;
	entry->next = new_node;
}

// 插入一个结点到entry前面
void list_insert_before(struct List * entry,struct List * new_node)
{
	ASSERT(entry != NULL);
	ASSERT(new_node != NULL);

	new_node->next = entry;
	entry->prev->next = new_node;
	new_node->prev = entry->prev;
	entry->prev = new_node;
}

void list_delete(struct List * entry)
{
	entry->next->prev = entry->prev;
	entry->prev->next = entry->next;
}

long list_is_empty(struct List * entry)
{
	if(entry == entry->next && entry->prev == entry)
		return 1;
	else
		return 0;
}

struct List * list_prev(struct List * entry)
{
	if(entry->prev != NULL)
		return entry->prev;
	else
		return NULL;
}

struct List * list_next(struct List * entry)
{
	if(entry->next != NULL)
		return entry->next;
	else
		return NULL;
}