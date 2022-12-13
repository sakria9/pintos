#include "frame.h"
#include "hash.h"
#include "threads/init.h"
#include "threads/loader.h"
#include "threads/palloc.h"

static struct hash frame_table;

static unsigned frame_hash(const struct hash_elem *e, void *aux UNUSED)
{
    const struct frame_element *f = hash_entry (e, struct frame_element, frame_table_elem);
    return hash_bytes(&f->kpage,sizeof(void*));
}

static bool frame_less(const struct hash_elem *a,const struct hash_elem *b, void *aux UNUSED)
{
    const struct frame_element *x = hash_entry(a, struct frame_element, frame_table_elem);
    const struct frame_element *y = hash_entry(b, struct frame_element, frame_table_elem);
    return x->kpage<y->kpage;
}

void frame_init(void)
{
    hash_init(&frame_table, frame_hash, frame_less, NULL);
}