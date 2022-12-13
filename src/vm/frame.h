#pragma once
#include "page.h"
#include <hash.h>

struct frame
{
    struct hash_elem frame_table_elem; // for frame_table
    void *kpage;
    struct page* upage;
};

void frame_table_init(void);
struct frame* frame_alloc(struct page*);
void frame_free(struct frame*);