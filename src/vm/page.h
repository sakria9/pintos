#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "filesys/off_t.h"
#include "hash.h"

struct page
{
    void *uaddr; //user virtual address for this page
    struct frame* frame;
    bool rw; // is writable
    bool is_stack; //used to check stack growth

    struct hash_elem page_table_elem; // for page_table

    struct file* file;
    off_t file_offset;
    size_t file_size; // [file_offset, file_offset + file_size) is mapped to this page

    size_t swap_index; // Where is this page in swap disk. When this page is not in swap, swap_index=BITMAP_ERROR

};

void page_table_init(struct hash*);
void page_table_destroy(struct hash*); 
struct page* page_create_stack(struct hash *page_table, void *uaddr);
struct page* page_create_not_stack(struct hash *page_table, void *uaddr, bool writeable, struct file* file, off_t offset, size_t size);
struct page* page_find(struct hash *page_table, void *uaddr);
void page_table_free_page(struct hash*, struct page*);
bool page_fault_handler(struct hash *page_table, void *uaddr, void* esp,bool read_only);
