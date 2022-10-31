#include "filesys/file.h"
#include "filesys/inode.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include <debug.h>
#include <stdio.h>

/* An open file. */
struct file
{
  struct inode *inode; /* File's inode. */
  off_t pos;           /* Current position. */
  bool deny_write;     /* Has file_deny_write() been called? */
};

static struct lock filesys_lock; /* Lock for file system operations. */
void
filesys_lock_init (void)
{
  lock_init (&filesys_lock);
}
void
filesys_lock_acquire (void)
{
  lock_acquire (&filesys_lock);
}
void
filesys_lock_release (void)
{
  lock_release (&filesys_lock);
}
int
filesys_lock_is_held_by_current_thread (void)
{
  return lock_held_by_current_thread (&filesys_lock);
}

/* Opens a file for the given INODE, of which it takes ownership,
   and returns the new file.  Returns a null pointer if an
   allocation fails or if INODE is null. */
struct file *
file_open (struct inode *inode)
{
  FILESYS_LOCK ();
  struct file *file = calloc (1, sizeof *file);
  if (inode != NULL && file != NULL)
    {
      file->inode = inode;
      file->pos = 0;
      file->deny_write = false;
      FILESYS_UNLOCK ();
      return file;
    }
  else
    {
      inode_close (inode);
      free (file);
      FILESYS_UNLOCK ();
      return NULL;
    }
}

/* Opens and returns a new file for the same inode as FILE.
   Returns a null pointer if unsuccessful. */
struct file *
file_reopen (struct file *file)
{
  FILESYS_LOCK ();
  struct file *new_file = file_open (inode_reopen (file->inode));
  FILESYS_UNLOCK ();
  return new_file;
}

/* Closes FILE. */
void
file_close (struct file *file)
{
  if (file != NULL)
    {
      FILESYS_LOCK ();
      file_allow_write (file);
      inode_close (file->inode);
      free (file);
      FILESYS_UNLOCK ();
    }
}

/* Returns the inode encapsulated by FILE. */
struct inode *
file_get_inode (struct file *file)
{
  return file->inode;
}

/* Reads SIZE bytes from FILE into BUFFER,
   starting at the file's current position.
   Returns the number of bytes actually read,
   which may be less than SIZE if end of file is reached.
   Advances FILE's position by the number of bytes read. */
off_t
file_read (struct file *file, void *buffer, off_t size)
{
  FILESYS_LOCK ();
  off_t bytes_read = inode_read_at (file->inode, buffer, size, file->pos);
  file->pos += bytes_read;
  FILESYS_UNLOCK ();
  return bytes_read;
}

/* Reads SIZE bytes from FILE into BUFFER,
   starting at offset FILE_OFS in the file.
   Returns the number of bytes actually read,
   which may be less than SIZE if end of file is reached.
   The file's current position is unaffected. */
off_t
file_read_at (struct file *file, void *buffer, off_t size, off_t file_ofs)
{
  FILESYS_LOCK ();
  off_t bytes_read = inode_read_at (file->inode, buffer, size, file_ofs);
  FILESYS_UNLOCK ();
  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into FILE,
   starting at the file's current position.
   Returns the number of bytes actually written,
   which may be less than SIZE if end of file is reached.
   (Normally we'd grow the file in that case, but file growth is
   not yet implemented.)
   Advances FILE's position by the number of bytes read. */
off_t
file_write (struct file *file, const void *buffer, off_t size)
{
  FILESYS_LOCK ();
  off_t bytes_written = inode_write_at (file->inode, buffer, size, file->pos);
  file->pos += bytes_written;
  FILESYS_UNLOCK ();
  return bytes_written;
}

/* Writes SIZE bytes from BUFFER into FILE,
   starting at offset FILE_OFS in the file.
   Returns the number of bytes actually written,
   which may be less than SIZE if end of file is reached.
   (Normally we'd grow the file in that case, but file growth is
   not yet implemented.)
   The file's current position is unaffected. */
off_t
file_write_at (struct file *file, const void *buffer, off_t size,
               off_t file_ofs)
{
  FILESYS_LOCK ();
  off_t bytes_written = inode_write_at (file->inode, buffer, size, file_ofs);
  FILESYS_UNLOCK ();
  return bytes_written;
}

/* Prevents write operations on FILE's underlying inode
   until file_allow_write() is called or FILE is closed. */
void
file_deny_write (struct file *file)
{
  ASSERT (file != NULL);
  FILESYS_LOCK ();
  if (!file->deny_write)
    {
      file->deny_write = true;
      inode_deny_write (file->inode);
    }
  FILESYS_UNLOCK ();
}

/* Re-enables write operations on FILE's underlying inode.
   (Writes might still be denied by some other file that has the
   same inode open.) */
void
file_allow_write (struct file *file)
{
  FILESYS_LOCK ();
  ASSERT (file != NULL);
  if (file->deny_write)
    {
      file->deny_write = false;
      inode_allow_write (file->inode);
    }
  FILESYS_UNLOCK ();
}

/* Returns the size of FILE in bytes. */
off_t
file_length (struct file *file)
{
  ASSERT (file != NULL);
  FILESYS_LOCK ();
  off_t length = inode_length (file->inode);
  FILESYS_UNLOCK ();
  return length;
}

/* Sets the current position in FILE to NEW_POS bytes from the
   start of the file. */
void
file_seek (struct file *file, off_t new_pos)
{
  ASSERT (file != NULL);
  ASSERT (new_pos >= 0);
  FILESYS_LOCK ();
  file->pos = new_pos;
  FILESYS_UNLOCK ();
}

/* Returns the current position in FILE as a byte offset from the
   start of the file. */
off_t
file_tell (struct file *file)
{
  ASSERT (file != NULL);
  FILESYS_LOCK ();
  off_t pos = file->pos;
  FILESYS_UNLOCK ();
  return pos;
}
