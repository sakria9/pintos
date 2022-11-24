#include "page.h"
#include "frame.h"
#include "debug.h"
#include "hash.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "stdlib.h"
#include "stdio.h"
#include "user/syscall.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"

#define STACK_LIMIT 8388608 // 8MB

static bool addr_is_stack(void *addr, void *esp)
{
    return addr >= PHYS_BASE-STACK_LIMIT && addr >= esp - 32;
}

static unsigned page_hash(const struct hash_elem *e, void *aux UNUSED)
{
    const struct page *p = hash_entry (e, struct page, page_table_elem);
    return hash_bytes(&p->uaddr,sizeof(void*));
}

static bool page_less(const struct hash_elem *a,const struct hash_elem *b, void *aux UNUSED)
{
    const struct page *x = hash_entry(a, struct page, page_table_elem);
    const struct page *y = hash_entry(b, struct page, page_table_elem);
    return x->uaddr<y->uaddr;
}

void page_table_init(struct hash* page_table){
    hash_init(page_table, page_hash, page_less, NULL);
}

void page_table_destroy(struct hash* page_table)
{
    hash_destroy(page_table, NULL);
}

// Create a new page.
// return NULL when failed
struct page* page_create(struct hash *page_table, void *uaddr)
{
    struct page *page = malloc(sizeof(struct page));
    if (page==NULL) {
        puts("No enough kernel memory!");
        return NULL;
    }
    page->uaddr = pg_round_down(uaddr);
    page->frame = frame_alloc(page);
    if (page->frame==NULL) {
        free(page);
        return NULL;
    }
    if (hash_insert(page_table, &page->page_table_elem)!=NULL) {
        puts("Page already exists! This should not happen!");
        ASSERT(false);
    }
    memset(page->frame->kpage, 0, PGSIZE); // zero the page
    page->is_stack=true;
    return page;
}

// return the page containing the given user address
// return NULL if not found.
struct page* page_find(struct hash *page_table, void *uaddr)
{
    void *upage = pg_round_down(uaddr);
    struct page page;
    page.uaddr=upage;
    struct hash_elem *e=hash_find(page_table, &page.page_table_elem);
    if (e==NULL) return NULL;
    return hash_entry(e,struct page,page_table_elem);
}

void page_free(struct hash* page_table, struct page* page)
{
    hash_delete(page_table, &page->page_table_elem);
    free(page);
}
bool page_fault_handler(struct hash *page_table, void *addr, void* esp, bool rw)
{
    if (!is_user_vaddr(addr)) return false;
    //void *upage = pg_round_down(addr);
    struct page* page = page_find(page_table,addr);
    if (page==NULL) {
        // create a new page
        if (!addr_is_stack(addr, esp)) {
            // we continue only if it is a stack growth
            //puts("Try to growth a non-stack memory"); 
            //printf("%llu %llu\n",(unsigned long long)addr,(unsigned long long)esp);
            return false;
        }
        page = page_create(page_table, addr); 
        page->rw=rw;
    }
    struct thread *t = thread_current();
    if (!pagedir_set_page(t->pagedir, page->uaddr, page->frame->kpage, rw)) {
        puts("Failed to add page into pagedir!");
        frame_free(page->frame);
        page_free(page_table,page);
        return false;
    }
    return true;
}