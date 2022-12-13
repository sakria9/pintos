#pragma once
#include <stdbool.h>

struct page
{
    void *upage;
    struct frame* frame;
    bool read_only;
};