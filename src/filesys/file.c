#include "filesys/file.h"
#include "filesys/inode.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include <debug.h>

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

/* Prevents write operations on FILE's underlying inode
   until file_allow_write() is called or FILE is closed. */
void
file_deny_write (struct file *file)
{
  ASSERT (file != NULL);
  filesys_lock_acquire ();
  if (!file->deny_write)
    {
      file->deny_write = true;
      inode_deny_write (file->inode);
    }
  filesys_lock_release ();
}

/* Re-enables write operations on FILE's underlying inode.
   (Writes might still be denied by some other file that has the
   same inode open.) */
static void
file_allow_write_without_lock (struct file *file)
{
  ASSERT (file != NULL);
  if (file->deny_write)
    {
      file->deny_write = false;
      inode_allow_write (file->inode);
    }
}
void
file_allow_write (struct file *file)
{
  filesys_lock_acquire ();
  file_allow_write_without_lock (file);
  filesys_lock_release ();
}

/* Opens a file for the given INODE, of which it takes ownership,
   and returns the new file.  Returns a null pointer if an
   allocation fails or if INODE is null. */
static struct file *
file_open_without_lock (struct inode *inode)
{
  struct file *file = calloc (1, sizeof *file);
  if (inode != NULL && file != NULL)
    {
      file->inode = inode;
      file->pos = 0;
      file->deny_write = false;
      return file;
    }
  else
    {
      inode_close (inode);
      free (file);
      return NULL;
    }
}
struct file *
file_open (struct inode *inode)
{
  filesys_lock_acquire ();
  struct file *file = file_open_without_lock (inode);
  filesys_lock_release ();
  return file;
}

/* Opens and returns a new file for the same inode as FILE.
   Returns a null pointer if unsuccessful. */
struct file *
file_reopen (struct file *file)
{
  filesys_lock_acquire ();
  struct file *new_file = file_open_without_lock (inode_reopen (file->inode));
  filesys_lock_release ();
  return new_file;
}

/* Closes FILE. */
void
file_close (struct file *file)
{
  if (file != NULL)
    {
      filesys_lock_acquire ();
      file_allow_write_without_lock (file);
      inode_close (file->inode);
      free (file);
      filesys_lock_release ();
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
  filesys_lock_acquire ();
  off_t bytes_read = inode_read_at (file->inode, buffer, size, file->pos);
  file->pos += bytes_read;
  filesys_lock_release ();
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
  filesys_lock_acquire ();
  off_t bytes_read = inode_read_at (file->inode, buffer, size, file_ofs);
  filesys_lock_release ();
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
  filesys_lock_acquire ();
  off_t bytes_written = inode_write_at (file->inode, buffer, size, file->pos);
  file->pos += bytes_written;
  filesys_lock_release ();
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
  filesys_lock_acquire ();
  off_t bytes_written = inode_write_at (file->inode, buffer, size, file_ofs);
  filesys_lock_release ();
  return bytes_written;
}

/* Returns the size of FILE in bytes. */
off_t
file_length (struct file *file)
{
  ASSERT (file != NULL);
  filesys_lock_acquire ();
  off_t length = inode_length (file->inode);
  filesys_lock_release ();
  return length;
}

/* Sets the current position in FILE to NEW_POS bytes from the
   start of the file. */
void
file_seek (struct file *file, off_t new_pos)
{
  ASSERT (file != NULL);
  ASSERT (new_pos >= 0);
  filesys_lock_acquire ();
  file->pos = new_pos;
  filesys_lock_release ();
}

/* Returns the current position in FILE as a byte offset from the
   start of the file. */
off_t
file_tell (struct file *file)
{
  ASSERT (file != NULL);
  filesys_lock_acquire ();
  off_t pos = file->pos;
  filesys_lock_release ();
  return pos;
}
