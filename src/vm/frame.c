#include "frame.h"
#include "hash.h"
#include "threads/init.h"
#include "threads/loader.h"
#include "threads/palloc.h"
#include <stdio.h>
#include "threads/malloc.h"

static struct hash frame_table;

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
}

struct frame* frame_alloc(struct page* upage)
{
    void *kpage = palloc_get_page(PAL_USER);
    if (kpage==NULL) {
        puts("No enough memory!"); //TODO: swap
        return NULL;
    }
    struct frame *frame = malloc(sizeof(struct frame));  
    if (frame==NULL) {
        puts("No enough kernel memory!");
        return NULL;
    }
    frame->kpage=kpage;
    frame->upage=upage; 
    hash_insert(&frame_table,&frame->frame_table_elem);
    return frame;
}

void frame_free(struct frame* frame)
{
    hash_delete(&frame_table, &frame->frame_table_elem);
    palloc_free_page(frame->kpage);
    free(frame);
}
