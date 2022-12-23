#pragma once
#include <devices/block.h>
#include <stdint.h>
#include <stdbool.h>
#include <threads/synch.h>

#define CACHE_SIZE 64
#define CACHE_UNUSED 1145141919

// File system cache of a sector
struct cache
{
    block_sector_t sector_id; // The sector id of this cache
    bool dirty; // Whether this cache is different from the disk
    bool second_chance; // For second chance algorithm
    uint8_t data[BLOCK_SECTOR_SIZE]; // The data of this cache
    struct lock lock; // lock it while reading or writing cache
};

extern struct lock cache_global_lock;
extern struct semaphore write_behind_stopped; // Used to wait for write-behind thread to stop.

void cache_init(void);
void cache_read(block_sector_t, void*, int,int);
void cache_write(block_sector_t, const void*,int,int);
struct cache* cache_find(block_sector_t);
struct cache* cache_new(block_sector_t);
struct cache* cache_evict(void);

void read_ahead(block_sector_t);

void cache_write_back(void);