#pragma once
#include "page.h"
#include <hash.h>

struct frame_element
{
    struct hash_elem frame_table_elem; // for frame_table
    void *kpage;
    struct page* upage;
};

void frame_init(void);