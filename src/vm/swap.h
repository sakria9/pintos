#pragma once
#include "page.h"
#include <stdbool.h>
#include <stddef.h>

void swap_init (void);
void swap_in (struct page *);
bool swap_out (struct page *);
void swap_free (size_t);
