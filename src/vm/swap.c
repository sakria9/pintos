#include "swap.h"
#include "devices/block.h"
#include "frame.h"
#include "page.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include <bitmap.h>

#define PAGE_BLOCKS (PGSIZE / BLOCK_SECTOR_SIZE)

static struct block *swap_device;
static struct bitmap *swap_bitmap;
static struct lock swap_lock;

void
swap_init (void)
{
  swap_device = block_get_role (BLOCK_SWAP);
  swap_bitmap = bitmap_create (block_size (swap_device) / PAGE_BLOCKS);
  lock_init (&swap_lock);
}

// Read the page from the swap slot
void
swap_in (struct page *page)
{
  // puts("swap in!");
  lock_acquire (&swap_lock);
  ASSERT (bitmap_test (swap_bitmap, page->swap_index));
  for (int i = 0; i < PAGE_BLOCKS; i++)
    {
      block_read (swap_device, page->swap_index * PAGE_BLOCKS + i,
                  page->frame->kpage + i * BLOCK_SECTOR_SIZE);
    }
  bitmap_reset (swap_bitmap, page->swap_index);
  page->swap_index = BITMAP_ERROR;
  lock_release (&swap_lock);
}

// Write the page to the swap slot
// Return whether the swap is successful
bool
swap_out (struct page *page)
{
  // puts("swap out!");
  lock_acquire (&swap_lock);
  page->swap_index = bitmap_scan_and_flip (swap_bitmap, 0, 1, false);
  if (page->swap_index == BITMAP_ERROR)
    {
      lock_release (&swap_lock);
      return false;
    }
  for (int i = 0; i < PAGE_BLOCKS; i++)
    {
      block_write (swap_device, page->swap_index * PAGE_BLOCKS + i,
                   page->frame->kpage + i * BLOCK_SECTOR_SIZE);
    }
  page->frame = NULL;
  lock_release (&swap_lock);
  return true;
}

// Free the swap slot
void
swap_free (size_t id)
{
  lock_acquire (&swap_lock);
  bitmap_reset (swap_bitmap, id);
  lock_release (&swap_lock);
}
