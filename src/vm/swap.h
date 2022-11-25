#pragma once
#include <stddef.h>
#include <stdbool.h>
#include "page.h"

void swap_init(void);
void swap_in(struct page*);
bool swap_out(struct page*);
void swap_free(size_t);