#ifndef FILESYS_FILE_H
#define FILESYS_FILE_H

#include "filesys/off_t.h"

struct inode;

/* Lock for file system operations. */
void filesys_lock_init (void);
void filesys_lock_acquire (void);
void filesys_lock_release (void);
int filesys_lock_is_held_by_current_thread (void);
#define FILESYS_LOCK()                                                        \
  int __is_filesys_locked = filesys_lock_is_held_by_current_thread ();        \
  if (!__is_filesys_locked)                                                   \
    filesys_lock_acquire ();
#define FILESYS_UNLOCK()                                                      \
  if (!__is_filesys_locked)                                                   \
    filesys_lock_release ();

/* Opening and closing files. */
struct file *file_open (struct inode *);
struct file *file_reopen (struct file *);
void file_close (struct file *);
struct inode *file_get_inode (struct file *);

/* Reading and writing. */
off_t file_read (struct file *, void *, off_t);
off_t file_read_at (struct file *, void *, off_t size, off_t start);
off_t file_write (struct file *, const void *, off_t);
off_t file_write_at (struct file *, const void *, off_t size, off_t start);

/* Preventing writes. */
void file_deny_write (struct file *);
void file_allow_write (struct file *);

/* File position. */
void file_seek (struct file *, off_t);
off_t file_tell (struct file *);
off_t file_length (struct file *);

#endif /* filesys/file.h */
