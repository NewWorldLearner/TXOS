#ifndef _PROCESS_H 
#define _PROCESS_H

#include "thread.h"

void create_user_vaddr_bitmap(struct task_struct *task);
uint64_t *create_page_dir();
void page_dir_activate(struct task_struct *task);
uint64_t start_process(uint64_t *user_process);
void process_creat(uint64_t *user_process);

#endif