#include "page.h"
#include "bitmap.h"
#include "debug.h"
#include "filesys/file.h"
#include "frame.h"
#include "hash.h"
#include "stddef.h"
#include "stdio.h"
#include "stdlib.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "user/syscall.h"
#include "userprog/pagedir.h"
#include "vm/swap.h"
#include <string.h>

#define STACK_LIMIT 8388608 // 8MB

static bool
addr_is_stack (void *addr, void *esp)
{
  return addr >= PHYS_BASE - STACK_LIMIT && addr >= esp - 32;
}

static unsigned
page_hash (const struct hash_elem *e, void *aux UNUSED)
{
  const struct page *p = hash_entry (e, struct page, page_table_elem);
  return hash_bytes (&p->uaddr, sizeof (void *));
}

static bool
page_less (const struct hash_elem *a, const struct hash_elem *b,
           void *aux UNUSED)
{
  const struct page *x = hash_entry (a, struct page, page_table_elem);
  const struct page *y = hash_entry (b, struct page, page_table_elem);
  return x->uaddr < y->uaddr;
}

void
page_table_init (struct hash *page_table)
{
  hash_init (page_table, page_hash, page_less, NULL);
}
// extern struct hash frame_table;

extern struct lock frame_global_lock;
void
page_table_destroy (struct hash *page_table)
{
  lock_acquire(&frame_global_lock);
  while (page_table->elem_cnt)
    {
      struct hash_iterator it;
      hash_first (&it, page_table);
      hash_next (&it);
      struct page *page
          = hash_entry (hash_cur (&it), struct page, page_table_elem);
      page_table_free_page (page_table, page);
    }
  lock_release(&frame_global_lock);
  hash_destroy (page_table, NULL);
}


// Create a new page.
// return NULL when failed
struct page *
page_create_stack (struct hash *page_table, void *uaddr)
{
  struct page *page = malloc (sizeof (struct page));
  if (page == NULL)
    {
      puts ("No enough kernel memory!");
      return NULL;
    }
  page->uaddr = pg_round_down (uaddr);
  page->frame = frame_alloc (page);
  page->file = NULL;
  if (page->frame == NULL)
    {
      free (page);
      return NULL;
    }
  if (hash_insert (page_table, &page->page_table_elem) != NULL)
    {
      puts ("Page already exists! This should not happen!");
      ASSERT (false);
    }
  memset (page->frame->kpage, 0, PGSIZE); // zero the page
  page->is_stack = true;
  page->swap_index = BITMAP_ERROR;
  return page;
}

struct page *
page_create_not_stack (struct hash *page_table, void *uaddr, bool writeable,
                       struct file *file, off_t offset, size_t size)
{
  struct page *page = malloc (sizeof (struct page));
  if (page == NULL)
    {
      puts ("No enough kernel memory!");
      return NULL;
    }
  page->uaddr = pg_round_down (uaddr);
  page->frame = NULL;

  page->rw = writeable;
  page->is_stack = false;

  page->file = file;
  page->file_offset = offset;
  page->file_size = size;

  page->swap_index = BITMAP_ERROR;

  if (hash_insert (page_table, &page->page_table_elem) != NULL)
    {
      puts ("Page already exists! This should not happen!");
      ASSERT (false);
    }
  return page;
}

// return the page containing the given user address
// return NULL if not found.
struct page *
page_find (struct hash *page_table, void *uaddr)
{
  void *upage = pg_round_down (uaddr);
  struct page page;
  page.uaddr = upage;
  struct hash_elem *e = hash_find (page_table, &page.page_table_elem);
  if (e == NULL)
    return NULL;
  return hash_entry (e, struct page, page_table_elem);
}

void
page_table_free_page (struct hash *page_table, struct page *page)
{
  if (page->file == NULL)
    {
      // ordinary page
      if (page->frame == NULL)
        {
          if (page->swap_index != BITMAP_ERROR)
            swap_free (page->swap_index);
        }
      else
        {
          ASSERT (page->swap_index == BITMAP_ERROR);
          frame_free (page->frame);
          page->frame = NULL;
        }
    }
  else
    {
      // mmap page
      if (page->frame == NULL)
        {
          if (page->swap_index != BITMAP_ERROR)
            {
              // TODO: read back
              ASSERT (0);
            }
        }
      else
        {
          ASSERT (page->swap_index == BITMAP_ERROR);
          file_write_at (page->file, page->uaddr, page->file_size,
                         page->file_offset);
          frame_free (page->frame);
          page->frame = NULL;
        }
    }
  hash_delete (page_table, &page->page_table_elem);
  free (page);
}

extern struct lock frame_global_lock;
bool
page_fault_handler (struct hash *page_table, void *addr, void *esp, bool rw)
{
  // printf("page fault: %p\n",pg_round_down(addr));
  if (addr == 0 || !is_user_vaddr (addr))
    {
      // puts("Invalid address!") ;
      // ASSERT(false);
      return false;
    }
  lock_acquire (&frame_global_lock);
  // void *upage = pg_round_down(addr);
  struct page *page = page_find (page_table, addr);
  if (page == NULL)
    {
      // create a new page
      // printf("%p %p\n",addr,esp);
      if (!addr_is_stack (addr, esp))
        {
          // we continue only if it is a stack growth
          // puts("Try to growth a non-stack memory");
          // ASSERT(false);
          // printf("lock_release: %p\n",addr);
          lock_release (&frame_global_lock);
          return false;
        }
      // puts("page fault: grow stack");
      page = page_create_stack (page_table, addr);
      page->rw = true;
    }
  else
    {
      if (rw && !page->rw)
        { // write to a read-only page
          // puts("Try to write to a read-only page");
          // ASSERT(false);
          // printf("lock_release: %p\n",addr);
          lock_release (&frame_global_lock);
          return false;
        }
      if (!addr_is_stack (addr, esp) && page->is_stack)
        { // stack overflow
          // puts("Stack overflow!");
          // ASSERT(false);
          //  printf("lock_release: %p\n",addr);
          lock_release (&frame_global_lock);
          return false;
        }
      if (page->swap_index != BITMAP_ERROR)
        {
          // puts("page fault: swap");
          page->frame = frame_alloc (page);
          swap_in (page);
        }
      else if (page->file)
        {
          // puts("page fault: file");
          page->frame = frame_alloc (page);
          ASSERT (page->frame);
          size_t actual_read
              = file_read_at (page->file, page->frame->kpage, page->file_size,
                              page->file_offset);
          ASSERT (actual_read == page->file_size);
          memset (page->frame->kpage + page->file_size, 0,
                  PGSIZE - page->file_size);
        }
      else
        {
          // lazy create
          ASSERT (page->frame == NULL);
          page->frame = frame_alloc (page);
          ASSERT (page->frame);
          memset (page->frame->kpage, 0, PGSIZE); // zero the page
        }
    }
  struct thread *t = thread_current ();
  if (!pagedir_set_page (t->pagedir, page->uaddr, page->frame->kpage,
                         page->rw))
    {
      // puts("Failed to add page into pagedir!");
      ASSERT (false);
      page_table_free_page (page_table, page);
      lock_release (&frame_global_lock);
      return false;
    }
  // printf("lock_release: %p\n",addr);
  lock_release (&frame_global_lock);
  return true;
}
