#pragma once
#include <stdbool.h>
#include "hash.h"

struct page
{
    void *uaddr; //user virtual address for this page
    struct frame* frame;
    bool rw; // is writable
    bool is_stack; //used to check stack growth

    struct hash_elem page_table_elem; // for page_table

    size_t swap_index; // Where is this page in swap disk. When this page is not in swap, swap_index=BITMAP_ERROR

};

void page_table_init(struct hash*);
void page_table_destroy(struct hash*); 
struct page* page_create(struct hash *page_table, void *uaddr);
struct page* page_find(struct hash *page_table, void *uaddr);
void page_free(struct hash*, struct page*);
bool page_fault_handler(struct hash *page_table, void *uaddr, void* esp,bool read_only);