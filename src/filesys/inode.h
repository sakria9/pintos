#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/block.h"

#define DIRECT_NUM 124
#define INDIRECT_NUM 128

struct bitmap;

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
{
    off_t length;                       /* File size in bytes. */
    unsigned is_dir;                    /* 1 if directory, 0 if file. */
    unsigned magic;                     /* Magic number. */
    /* Our File System has 124 direct blocks, and 1 double-indirect block. 
       One double-indirect block can point to 128 indirect blocks, and each indirect block can point to 128 direct blocks.
       So the maximum number of blocks in one file is 124 + 128 * 128.
       Unlike ext2-like FS, we don't have a indirect block, because the number of direct blocks is very large.
    */
    block_sector_t direct[DIRECT_NUM]; // point to direct data blocks
    block_sector_t indirect; // point to DOUBLE-indirect data blocks
};

// On-disk indirect block
struct indirect_inode
{
  block_sector_t data[INDIRECT_NUM];
};

void inode_init (void);
bool inode_create (block_sector_t, off_t);
struct inode *inode_open (block_sector_t);
struct inode *inode_reopen (struct inode *);
block_sector_t inode_get_inumber (const struct inode *);
void inode_close (struct inode *);
void inode_remove (struct inode *);
off_t inode_read_at (struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at (struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write (struct inode *);
void inode_allow_write (struct inode *);
off_t inode_length (const struct inode *);
bool inode_is_dir(const struct inode *);
void inode_set_dir(struct inode *, bool is_dir);
int inode_open_cnt(const struct inode *);

bool alloc_inode_block(block_sector_t *block);
bool extend_inode(struct inode_disk*, int);
void free_inode(struct inode_disk*);

#endif /* filesys/inode.h */
