#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <stdint.h>
#include <string.h>
#include "devices/block.h"
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/cache.h"
#include "threads/synch.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

#define INVALID_SECTOR ((block_sector_t) -1)



/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
    struct lock lock; // inode IO lock
  };

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  int sector_pos = pos / BLOCK_SECTOR_SIZE;
  if (sector_pos < DIRECT_NUM) {
    block_sector_t ret = inode->data.direct[sector_pos];
    check_sector(fs_device, ret);
    return ret;
  }

  sector_pos -= DIRECT_NUM;
  int pos_1 = sector_pos / INDIRECT_NUM;
  int pos_2 = sector_pos % INDIRECT_NUM;
  if (pos_1 >= INDIRECT_NUM) {
    return INVALID_SECTOR;
  }

  struct indirect_inode *d_indirect = malloc(sizeof(struct indirect_inode));
  cache_read(inode->data.indirect, d_indirect, 0, BLOCK_SECTOR_SIZE);

  if (d_indirect->data[pos_1] == 0) {
    free(d_indirect);
    return INVALID_SECTOR;
  }

  struct indirect_inode *indirect = malloc(sizeof(struct indirect_inode));
  cache_read(d_indirect->data[pos_1], indirect, 0, BLOCK_SECTOR_SIZE);

  block_sector_t ret = -1;
  if (indirect->data[pos_2]!=0) {
    ret = indirect->data[pos_2];
  }
  check_sector(fs_device, ret);
  free(d_indirect);
  free(indirect);
  return ret;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      if (extend_inode(disk_inode, sectors)) {
        cache_write(sector, disk_inode, 0, BLOCK_SECTOR_SIZE);
        success = true;
      }
      free (disk_inode);
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  lock_init(&inode->lock);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  //block_read (fs_device, inode->sector, &inode->data);
  cache_read(inode->sector, &inode->data, 0, BLOCK_SECTOR_SIZE);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
          free_inode(&inode->data);
        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  lock_acquire(&inode->lock);
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;

  if (offset > inode->data.length)
    size = 0;
  else if (offset + size > inode->data.length)
    size = inode->data.length - offset;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      ASSERT(sector_idx != 0 && sector_idx != INVALID_SECTOR);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      // Simply read data from cache.
      cache_read (sector_idx, buffer + bytes_read, sector_ofs, chunk_size);
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }

  lock_release(&inode->lock);
  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  lock_acquire(&inode->lock);

  // Extend inode if necessary.
  if (offset + size > inode->data.length) {
    int sectors = bytes_to_sectors(offset + size);
    if (!extend_inode(&inode->data, sectors)) {
      lock_release(&inode->lock);
      return 0;
    }
    inode->data.length = offset + size;
    cache_write(inode->sector, &inode->data, 0, BLOCK_SECTOR_SIZE);
  }

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      ASSERT(sector_idx != 0 && sector_idx != INVALID_SECTOR);
      check_sector(fs_device, sector_idx);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      // Write data to cache.
      // We do not need to read data first, because it will be done in cache.
      cache_write(sector_idx, buffer + bytes_written, sector_ofs, chunk_size);      

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  lock_release(&inode->lock);
  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}

bool inode_is_dir(const struct inode * inode) {
  return inode->data.is_dir;
}

void inode_set_dir(struct inode * inode, bool is_dir) {
  inode->data.is_dir = is_dir;
  // block_write (fs_device, inode->sector, &inode->data);
  cache_write(inode->sector, &inode->data, 0, BLOCK_SECTOR_SIZE);
}

int inode_open_cnt(const struct inode * inode) {
  return inode->open_cnt;
}

// Alloc a block for inode.
bool alloc_inode_block(block_sector_t *block)
{
  static uint8_t zeros[BLOCK_SECTOR_SIZE];
  if(*block==0)
  {
    if(!free_map_allocate(1, block))
      return false;
    cache_write(*block, zeros, 0, BLOCK_SECTOR_SIZE);
  }
  return true;
}

// extend inode's data blocks to <n>.
// if <n> is smaller than current data blocks, do nothing.
bool extend_inode(struct inode_disk* inode, int n)
{
  int n_direct = (n<=DIRECT_NUM) ? n : DIRECT_NUM;
  n-= n_direct;
  

  for(int i=0; i<n_direct; i++)
  {
    if (!alloc_inode_block(&inode->direct[i])) // allocate direct block
      return false;
  }

  if (n==0) return true;

  int n_indirect = n/INDIRECT_NUM;
  if (n%(INDIRECT_NUM)!=0)
    n_indirect++;
  
  //double-indirect block
  struct indirect_inode *d_indirect_block = malloc(sizeof(struct indirect_inode));
  
  if (inode->indirect==0) {
    if (!alloc_inode_block(&inode->indirect)) { // allocate double-indirect block
      free(d_indirect_block);
      return false;
    }
    memset(d_indirect_block, 0, sizeof(struct indirect_inode));
  } else {
    cache_read(inode->indirect, d_indirect_block, 0, BLOCK_SECTOR_SIZE);
  }

  for(int i=0; i<n_indirect; i++)
  {
    int n_now = (n<=INDIRECT_NUM) ? n : INDIRECT_NUM;
    n-= n_now;

    struct indirect_inode *indirect_block = malloc(sizeof(struct indirect_inode));
    block_sector_t *now_sector = &d_indirect_block->data[i];
    if (*now_sector==0) {
      if (!alloc_inode_block(now_sector)) { // allocate indirect block
        free(d_indirect_block);
        free(indirect_block);
        return false;
      }
      memset(indirect_block, 0, sizeof(struct indirect_inode));
    } else {
      cache_read(*now_sector, indirect_block, 0, BLOCK_SECTOR_SIZE);
    }
    for(int j=0; j<n_now; j++)
    {
      if (!alloc_inode_block(&indirect_block->data[j])) { // allocate block in double-indirect block
        free(d_indirect_block);
        free(indirect_block);
        return false;
      }
    }
    cache_write(*now_sector, indirect_block, 0, BLOCK_SECTOR_SIZE);
    free(indirect_block);
  }
  cache_write(inode->indirect, d_indirect_block, 0, BLOCK_SECTOR_SIZE);
  free(d_indirect_block);
  return true;
}

// delete an inode from disk. Garbage Collection.
void free_inode(struct inode_disk* inode) 
{
  for(int i=0; i<DIRECT_NUM; i++)
  {
    if (inode->direct[i]!=0)
      free_map_release(inode->direct[i], 1);
  }
  struct indirect_inode d_indirect_block; // double-indirect block
  if (inode->indirect!=0) {
    cache_read(inode->indirect, &d_indirect_block, 0, BLOCK_SECTOR_SIZE);
    for(int i=0; i<INDIRECT_NUM; i++)
    {
      if (d_indirect_block.data[i]!=0) {
        struct indirect_inode indirect_block; // indirect block
        cache_read(d_indirect_block.data[i], &indirect_block, 0, BLOCK_SECTOR_SIZE);
        for(int j=0; j<INDIRECT_NUM; j++)
        {
          if (indirect_block.data[j]!=0)
            free_map_release(indirect_block.data[j], 1);
        }
        free_map_release(d_indirect_block.data[i], 1);
      }
    }
    free_map_release(inode->indirect, 1);
  }
} 