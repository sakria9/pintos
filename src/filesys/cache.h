#pragma once
#include <devices/block.h>
#include <stdint.h>
#include <stdbool.h>
#include <threads/synch.h>

#define CACHE_SIZE 64
#define CACHE_UNUSED 1145141919

struct cache
{
    block_sector_t sector_id;
    bool dirty;
    uint8_t data[BLOCK_SECTOR_SIZE];
    struct lock lock; // lock it while reading or writing cache
};

extern struct lock cache_global_lock;

void cache_init(void);
void cache_read(block_sector_t, void*, int,int);
void cache_write(block_sector_t, const void*,int,int);
struct cache* cache_find(block_sector_t);

void cache_write_back(void);