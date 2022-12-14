#include "cache.h"
#include "devices/block.h"
#include "threads/synch.h"
#include "devices/timer.h"
#include "filesys.h"
#include "threads/thread.h"
#include "string.h"

static struct cache cache[CACHE_SIZE];
struct lock cache_global_lock;

void write_behind(void)
{
    while(!filesystem_shutdown) {
        cache_write_back();
        timer_sleep(200);
    }
}

void cache_init(void)
{
    lock_init(&cache_global_lock);
    lock_acquire(&cache_global_lock);
    for(int i=0; i<CACHE_SIZE; i++) {
        cache[i].sector_id=CACHE_UNUSED;
        cache[i].dirty=false;
        lock_init(&cache[i].lock);
    }
    thread_create ("write-behind", PRI_DEFAULT, (thread_func *) write_behind, NULL);
    lock_release(&cache_global_lock);
}

// Write back all dirty cache.
void cache_write_back(void)
{
    lock_acquire(&cache_global_lock);
    for(int i=0; i<CACHE_SIZE; i++) {
        if (cache[i].sector_id==CACHE_UNUSED) {
            continue;
        }
        if (cache[i].dirty) {
            block_write(fs_device, cache[i].sector_id, cache[i].data);
            cache[i].dirty=false;
        }
    }
    lock_release(&cache_global_lock);
}

// Find the cache contains sector <id>
// Return NULL if not found.
struct cache* cache_find(block_sector_t id)
{
    for(int i=0; i<CACHE_SIZE; i++) {
        if (cache[i].sector_id == id) {
            return cache+i;
        }
    }
    return NULL;
}


// Load data from cache/disk to <data>
void cache_read(block_sector_t id, void* data, int offset,int size)
{
    lock_acquire(&cache_global_lock);
    struct cache * c=cache_find(id);
    if (c==NULL) {
        //TODO: read data from disk and then insert cache
    }
    lock_acquire(&c->lock);
    lock_release(&cache_global_lock); // it is safe to release the global lock when we have locked the cache's lock.
    memcpy(data,c->data+offset,size);
    lock_release(&c->lock);
}

// Write data from <data> to cache
void cache_write(block_sector_t id, const void* data,int offset,int size)
{
    lock_acquire(&cache_global_lock);
    struct cache * c=cache_find(id);
    if (c==NULL) {
        //TODO: insert cache
    }
    lock_acquire(&c->lock);
    lock_release(&cache_global_lock); // it is safe to release the global lock when we have locked the cache's lock.
    memcpy(c->data+offset,data,size);
    lock_release(&c->lock);
}