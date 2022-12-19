#include "cache.h"
#include "devices/block.h"
#include "threads/synch.h"
#include "devices/timer.h"
#include "filesys.h"
#include "threads/thread.h"
#include "string.h"

void write_behind(void);

static struct cache cache[CACHE_SIZE];
struct lock cache_global_lock;
struct semaphore write_behind_stopped; // Used to wait for write-behind thread to stop.

void write_behind(void)
{
    sema_init(&write_behind_stopped, 0);
    while(!filesystem_shutdown) {
        cache_write_back();
        timer_sleep(200);
    }
    sema_up(&write_behind_stopped);
}

void cache_init(void)
{
    lock_init(&cache_global_lock);
    lock_acquire(&cache_global_lock);
    for(int i=0; i<CACHE_SIZE; i++) {
        cache[i].sector_id=CACHE_UNUSED;
        cache[i].dirty=false;
        cache[i].second_chance=true;
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
            cache[i].second_chance=true;
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
        c = cache_new(id);
        lock_acquire(&c->lock);
        lock_release(&cache_global_lock); // it is safe to release the global lock when we have locked the cache's lock.
        block_read(fs_device,id, c->data);
    } else {
        lock_acquire(&c->lock);
        lock_release(&cache_global_lock); // it is safe to release the global lock when we have locked the cache's lock.
    }
    memcpy(data,c->data+offset,size);
    lock_release(&c->lock);
}

// Write data from <data> to cache
void cache_write(block_sector_t id, const void* data,int offset,int size)
{
    lock_acquire(&cache_global_lock);
    struct cache * c=cache_find(id);
    if (c==NULL) {
        c = cache_new(id);
        lock_acquire(&c->lock);
        lock_release(&cache_global_lock); // it is safe to release the global lock when we have locked the cache's lock.
        block_read(fs_device,id, c->data);
    } else {
        lock_acquire(&c->lock);
        lock_release(&cache_global_lock); // it is safe to release the global lock when we have locked the cache's lock.
    }
    c->dirty=true;
    memcpy(c->data+offset,data,size);
    lock_release(&c->lock);
}
struct cache* cache_new(block_sector_t id)
{
    struct cache *c=NULL;
    //lock_acquire(&cache_global_lock);
    for(int i=0; i<CACHE_SIZE; i++) {
        if (cache[i].sector_id==CACHE_UNUSED) {
            c=cache+i;
            break;
        }
    }
    if (c==NULL) {
        c=cache_evict();
    }
    if (c==NULL) {
        PANIC("cache_new: cannot find a cache entry");
    }
    c->second_chance=true;
    c->sector_id=id;
    c->dirty=false;
    //lock_release(&cache_global_lock);
    return c;
}

// Evict a cache entry.
// Should be called when all cache entries are used.
struct cache* cache_evict(void)
{
    struct cache *c=NULL;
    for(int k=1; k<=10; k++) { // try hard to find a cache entry.
        for(int i=0; i<CACHE_SIZE; i++) {
            if (lock_try_acquire(&cache[i].lock)) {
                if (cache[i].second_chance) {
                    cache[i].second_chance=false;
                    lock_release(&cache[i].lock);
                } else {
                    c=cache+i;
                    break;
                }
            }
        }
        if (c==NULL) {
            continue;
        }
        if (c->dirty) {
            block_write(fs_device, c->sector_id, c->data);
            c->dirty=false;
        }
        c->sector_id=CACHE_UNUSED;
        lock_release(&c->lock);
    }
    if (c==NULL) {
        PANIC("cache_evict: cannot find a cache entry");
    }
    return c;
}