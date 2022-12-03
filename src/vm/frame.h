#pragma once
#include "page.h"
#include "threads/thread.h"
#include <hash.h>

struct frame
{
  struct hash_elem frame_table_elem; // for frame_table
  void *kpage; // kernel virtual address for this frame
  struct page *upage; // corresponding page.
  struct thread *owner; // thread that owns this frame

  int second_change; // for Second Change algorithm
};

extern struct lock frame_global_lock;
void frame_table_init (void);
struct frame *frame_alloc (struct page *);
void frame_free (struct frame *);
bool frame_evict (void);
void frame_clear (struct frame *frame);
