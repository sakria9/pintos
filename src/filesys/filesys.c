#include "filesys/filesys.h"
#include "devices/timer.h"
#include "filesys/cache.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include <debug.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Partition that contains the file system. */
struct block *fs_device;

bool filesystem_shutdown; // Whether filesystem is closed.

static void do_format (void);
static void path_split (const char *path, char *dir, char *file);
static struct dir *try_open_directory (char *directory);
static bool try_open_parent_directory_and_get_basename (const char *name,
                                                        struct dir **d,
                                                        char **basename);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format)
{
  filesys_lock_init ();

  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  cache_init ();

  inode_init ();
  free_map_init ();

  if (format)
    do_format ();

  free_map_open ();
  filesystem_shutdown = false;
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void)
{
  FILESYS_LOCK ();
  cache_write_back ();
  filesystem_shutdown = true;
  sema_down (&write_behind_stopped); // Wait for write-behind thread to stop.
  free_map_close ();
  FILESYS_UNLOCK ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size)
{
  struct dir *parent_dir;
  char *basename;
  if (!try_open_parent_directory_and_get_basename (name, &parent_dir,
                                                   &basename))
    return false;

  FILESYS_LOCK ();
  block_sector_t inode_sector = 0;
  bool success = (parent_dir != NULL && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size)
                  && dir_add (parent_dir, basename, inode_sector));
  if (!success && inode_sector != 0)
    free_map_release (inode_sector, 1);
  dir_close (parent_dir);
  FILESYS_UNLOCK ();

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  struct dir *parent_dir;
  char *basename;
  if (!try_open_parent_directory_and_get_basename (name, &parent_dir,
                                                   &basename))
    return false;

  FILESYS_LOCK ();

  struct inode *inode = NULL;

  dir_lookup (parent_dir, basename, &inode);
  dir_close (parent_dir);

  struct file *file = file_open (inode);
  FILESYS_UNLOCK ();
  return file;
}

bool
filesys_open_file_or_directory (const char *name, struct file **f,
                                struct dir **d)
{
  if (name[0] == '/' && name[1] == '\0')
    {
      *f = NULL;
      *d = dir_open_root ();
      return true;
    }

  struct dir *parent_dir;
  char *basename;
  if (!try_open_parent_directory_and_get_basename (name, &parent_dir,
                                                   &basename))
    return false;
  // printf("open parent succ\n");

  FILESYS_LOCK ();
  *f = NULL, *d = NULL;

  struct inode *inode;
  dir_lookup (parent_dir, basename, &inode);
  dir_close (parent_dir);

  if (!inode)
    {
      FILESYS_UNLOCK ();
      return false;
    }

  if (inode_is_dir (inode))
    *d = dir_open (inode);
  else
    *f = file_open (inode);
  FILESYS_UNLOCK ();
  return true;
}

bool
filesys_mkdir (const char *name)
{
  struct dir *parent_dir;
  char *basename;
  if (!try_open_parent_directory_and_get_basename (name, &parent_dir,
                                                   &basename))
    return false;

  FILESYS_LOCK ();

  struct inode *inode;
  if (dir_lookup (parent_dir, basename, &inode))
    {
      inode_close (inode);
      dir_close (parent_dir);
      FILESYS_UNLOCK ();
      return false;
    }

  block_sector_t inode_sector = 0;
  free_map_allocate (1, &inode_sector);
  ASSERT (inode_sector != 0);
  ASSERT (dir_create (inode_sector, 16));

  struct dir *d = dir_open (inode_open (inode_sector));
  ASSERT (d);

  ASSERT (dir_add (d, ".", inode_sector));
  ASSERT (dir_add (parent_dir, basename, inode_sector));
  ASSERT (dir_add (d, "..", inode_get_inumber (dir_get_inode (parent_dir))));

  dir_close (d);
  dir_close (parent_dir);

  FILESYS_UNLOCK ();
  return true;
}

bool
filesys_chdir (const char *name)
{
  struct file *f;
  struct dir *d;
  FILESYS_LOCK ();
  if (!filesys_open_file_or_directory (name, &f, &d) || d == NULL)
    {
      FILESYS_UNLOCK ();
      return false;
    }
  if (thread_current ()->cwd != NULL)
    dir_close (thread_current ()->cwd);
  thread_current ()->cwd = d;
  FILESYS_UNLOCK ();
  return true;
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name)
{
  struct dir *parent_dir;
  char *basename;
  if (!try_open_parent_directory_and_get_basename (name, &parent_dir,
                                                   &basename))
    return false;
  FILESYS_LOCK ();
  struct inode *inode;
  dir_lookup (parent_dir, basename, &inode);

  bool success = false;

  if (!inode)
    {
      dir_close (parent_dir);
      FILESYS_UNLOCK ();
      return false;
    }
  if (inode_is_dir (inode))
    {
      struct dir *d = dir_open (inode);
      ASSERT (d);
      if (inode_open_cnt (dir_get_inode (d)) == 1 && dir_is_empty (d))
        success = dir_remove (parent_dir, basename);
      dir_close (d);
    }
  else
    {
      inode_close (inode);
      success = dir_remove (parent_dir, basename);
    }

  dir_close (parent_dir);
  FILESYS_UNLOCK ();
  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  FILESYS_LOCK ();
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  struct dir *root = dir_open_root ();
  ASSERT (dir_add (root, ".", ROOT_DIR_SECTOR));
  ASSERT (dir_add (root, "..", ROOT_DIR_SECTOR));
  dir_close (root);
  free_map_close ();
  printf ("done.\n");
  FILESYS_UNLOCK ();
}

static void
path_split (const char *path, char *dir, char *base)
{
  size_t path_len = strlen (path);
  const char *last_slash = strrchr (path, '/');
  if (last_slash == NULL)
    {
      strlcpy (dir, ".", path_len + 1);
      strlcpy (base, path, path_len + 1);
    }
  else
    {
      strlcpy (dir, path, last_slash - path + 1);
      strlcpy (base, last_slash + 1, path_len - (last_slash - path));
    }
  // printf ("path %s, dir: %s, file: %s\n", path, dir, base);
}

static struct dir *
try_open_directory (char *directory)
{
  struct dir *cur = NULL;
  if (thread_current ()->cwd)
    cur = dir_reopen (thread_current ()->cwd);
  size_t i = 0;
  if (directory[0] == '/' || directory[0] == '\0')
    {
      cur = dir_open_root ();
      while (directory[i] == '/')
        i++;
    }
  else if (cur == NULL)
    cur = dir_open_root ();
  size_t len = strlen (directory);
  while (i < len)
    {
      size_t j = i;
      while (j < len && directory[j] != '/')
        j++;
      directory[j] = '\0';
      struct inode *inode;
      if (!dir_lookup (cur, directory + i, &inode))
        {
          dir_close (cur);
          return NULL;
        }
      dir_close (cur);
      if (!inode_is_dir (inode))
        return NULL;
      cur = dir_open (inode);
      i = j + 1;
    }
  return cur;
}

static bool
try_open_parent_directory_and_get_basename (const char *name, struct dir **d,
                                            char **basename)
{
  size_t size = strlen (name) + 1;

  char *parent_dir_path = (char *)malloc (size);
  *basename = (char *)malloc (size);

  path_split (name, parent_dir_path, *basename);
  size_t basename_len = strlen (*basename);
  if (basename_len == 0)
    {
      // printf ("basename null\n");

      free (parent_dir_path);
      free (*basename);
      return false;
    }

  FILESYS_LOCK ();
  struct dir *parent_dir = try_open_directory (parent_dir_path);
  FILESYS_UNLOCK ();
  free (parent_dir_path);

  if (parent_dir == NULL)
    {
      // printf ("parent dir open failed: %s\n", name);

      free (*basename);
      return false;
    }

  *d = parent_dir;
  return true;
}
