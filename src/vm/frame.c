#include "frame.h"
#include "hash.h"
#include "threads/init.h"
#include "threads/loader.h"
#include "threads/palloc.h"
#include <stdio.h>
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "vm/swap.h"

static struct hash frame_table;
struct lock frame_global_lock;

static unsigned frame_hash(const struct hash_elem *e, void *aux UNUSED)
{
    const struct frame *f = hash_entry (e, struct frame, frame_table_elem);
    return hash_bytes(&f->kpage,sizeof(void*));
}

static bool frame_less(const struct hash_elem *a,const struct hash_elem *b, void *aux UNUSED)
{
    const struct frame *x = hash_entry(a, struct frame, frame_table_elem);
    const struct frame *y = hash_entry(b, struct frame, frame_table_elem);
    return x->kpage<y->kpage;
}

void frame_table_init(void)
{
    hash_init(&frame_table, frame_hash, frame_less, NULL);
    lock_init(&frame_global_lock);
}

struct frame* frame_alloc(struct page* upage)
{
    void *kpage = palloc_get_page(PAL_USER);
    if (kpage==NULL) {
        frame_evict(); // Try to empty a slot
        kpage = palloc_get_page(PAL_USER); // try again
        if (kpage==NULL) {
            ASSERT(false);
            return NULL;
        }
    }
    struct frame *frame = malloc(sizeof(struct frame));  
    if (frame==NULL) {
        puts("No enough kernel memory!");
        return NULL;
    }
    frame->kpage=kpage;
    frame->upage=upage; 
    hash_insert(&frame_table,&frame->frame_table_elem);
    frame->second_change = 1;
    frame->owner=thread_current();
    return frame;
}

void frame_free(struct frame* frame)
{
    hash_delete(&frame_table, &frame->frame_table_elem);
    pagedir_clear_page(frame->owner->pagedir, frame->upage->uaddr);
    palloc_free_page(frame->kpage);
    free(frame);
}

void frame_clear(struct frame* frame)
{
    hash_delete(&frame_table, &frame->frame_table_elem);
    palloc_free_page(frame->kpage);
    free(frame);
}


// Evit a page
// Return whether it succeed.
bool frame_evict(void)
{
    //lock_acquire(&frame_global_lock);
    // Second Chance (clock) algorithm
    struct hash_iterator it;
    hash_first(&it, &frame_table);
    struct frame* frame=NULL;
    while(hash_next(&it)) {
        frame = hash_entry(hash_cur(&it), struct frame, frame_table_elem); 
        if (frame->second_change==0) break;
        frame->second_change = 0;
    }
    if (frame==NULL) return false;
    swap_out(frame->upage);
    frame_free(frame);
    //lock_release(&frame_global_lock);
    return true;
}