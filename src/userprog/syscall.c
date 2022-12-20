#include "userprog/syscall.h"
#include "devices/input.h"
#include "devices/shutdown.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "stddef.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include <stdint.h>
#include <stdio.h>
#include <syscall-nr.h>

typedef int pid_t;          /* Corresponding to the documents */
int syscall_arg_number[30]; /* The number of arguments of every syscall */
void *syscall_func[30];     /* The function pointer of every syscall */

/* Get the file node by fd.
 * Returns the file_node address if successfull, NULL if not found. */
static struct file_node *get_file_node_by_fd (struct list *file_list, int fd);

/* Reads a byte at user virtual address UADDR.
   UADDR must be below PHYS_BASE.
   Returns the byte value if successful, -1 if a segfault
   occurred. */
static int
get_user (const uint8_t *uaddr)
{
  int result;
  asm("movl $1f, %0; movzbl %1, %0; 1:" : "=&a"(result) : "m"(*uaddr));
  return result;
}

/* Writes BYTE to user address UDST.
   UDST must be below PHYS_BASE.
   Returns true if successful, false if a segfault occurred. */
static bool
put_user (uint8_t *udst, uint8_t byte)
{
  int error_code;
  asm("movl $1f, %0; movb %b2, %1; 1:"
      : "=&a"(error_code), "=m"(*udst)
      : "q"(byte));
  return error_code != -1;
}

/* Check if int is readable */
static bool
check_int_get (int const *uaddr)
{
  return is_user_vaddr ((uint8_t *)uaddr)
         && is_user_vaddr ((uint8_t *)uaddr + 3)
         && get_user ((uint8_t *)uaddr) != -1
         && get_user ((uint8_t *)uaddr + 3) != -1;
}

/* Check if buffer is readable */
static bool
check_buffer_get (uint8_t const *buffer, unsigned size)
{
  return is_user_vaddr (buffer) && is_user_vaddr (buffer + size - 1)
         && get_user (buffer) != -1 && get_user (buffer + size - 1) != -1;
}

/* Check if buffer is writable */
static bool
check_buffer_put (uint8_t *buffer, unsigned size)
{
  return check_buffer_get (buffer, size)
         && put_user (buffer, get_user (buffer))
         && put_user (buffer + size - 1, get_user (buffer + size - 1));
}

/* Check if string is valid */
static bool
check_string (char const *str)
{
  while (1)
    {
      if (!is_user_vaddr (str))
        return false;
      int r = get_user ((uint8_t const *)str);
      if (r == 0)
        return true;
      if (r == -1)
        return false;
      str++;
    }
}

static void
exit (int status)
{
  thread_current ()->exit_status = status;
  thread_exit ();
}
static void
halt (void)
{
  shutdown_power_off ();
}
static pid_t
exec (const char *cmd_line)
{
  if (!check_string (cmd_line))
    exit (-1);
  return process_execute (cmd_line);
}
static int
wait (pid_t pid)
{
  return process_wait (pid);
}
static int
create (const char *file, unsigned initial_size)
{
  if (!check_string (file))
    exit (-1);
  // printf ("create syscall: %s\n", file);
  return filesys_create (file, initial_size);
}
static int
remove (const char *file)
{
  if (!check_string (file))
    exit (-1);
  return filesys_remove (file);
}
static int
open (const char *file)
{
  if (!check_string (file))
    exit (-1);
  // printf ("open syscall: %s\n", file);
  struct file *f;
  struct dir *d;

  if (!filesys_open_file_or_directory (file, &f, &d))
    return -1;

  // printf ("f: %p, d: %p\n", f, d);
  ASSERT (f != NULL || d != NULL);
  ASSERT (f == NULL || d == NULL);

  struct thread *t = thread_current ();
  struct file_node *file_node = malloc (sizeof (struct file_node));
  file_node->file = f;
  if (d)
    file_node->is_dir = true, file_node->dir = d;
  else
    file_node->is_dir = false, file_node->file = f;
  // printf("isdir: %d\n", (int)file_node->is_dir);
  file_node->fd = t->next_fd++;
  list_push_back (&t->file_list, &file_node->elem);
  return file_node->fd;
}
static int
filesize (int fd)
{
  struct file_node *file_node
      = get_file_node_by_fd (&thread_current ()->file_list, fd);
  if (file_node)
    return file_length (file_node->file);
  else
    exit (-1);
  NOT_REACHED ();
}
static int
read (int fd, void *buffer, unsigned size)
{
  if (!check_buffer_put (buffer, size))
    exit (-1);

  if (fd == 0)
    {
      uint8_t *buf = buffer;
      for (unsigned i = 0; i < size; i++)
        buf[i] = input_getc ();
      return size;
    }
  else
    {
      struct file_node *file_node
          = get_file_node_by_fd (&thread_current ()->file_list, fd);
      if (!file_node)
        return -1;
      if (file_node->is_dir)
        exit (-1);
      return file_read (file_node->file, buffer, size);
    }
}
static int
write (int fd, const void *buffer, unsigned size)
{
  if (!check_buffer_get (buffer, size))
    exit (-1);

  if (fd == 1)
    {
      putbuf (buffer, size);
      return size;
    }
  else
    {
      struct file_node *file_node
          = get_file_node_by_fd (&thread_current ()->file_list, fd);
      if (!file_node)
        exit (-1);
      if (file_node->is_dir)
        return -1;
      return file_write (file_node->file, buffer, size);
    }
}
static void
seek (int fd, unsigned position)
{
  struct file_node *file_node
      = get_file_node_by_fd (&thread_current ()->file_list, fd);
  if (file_node)
    file_seek (file_node->file, position);
  else
    exit (-1);
}
static unsigned
tell (int fd)
{
  struct file_node *file_node
      = get_file_node_by_fd (&thread_current ()->file_list, fd);
  if (file_node)
    return file_tell (file_node->file);
  else
    exit (-1);
  NOT_REACHED ();
}
static void
close (int fd)
{
  struct list *file_list = &thread_current ()->file_list;
  struct file_node *file_node = get_file_node_by_fd (file_list, fd);
  if (!file_node)
    exit (-1);
  if (file_node->is_dir)
    dir_close (file_node->dir);
  else
    file_close (file_node->file);
  list_remove (&file_node->elem);
  free (file_node);
}

static int
isdir (int fd)
{
  struct list *file_list = &thread_current ()->file_list;
  struct file_node *file_node = get_file_node_by_fd (file_list, fd);
  if (!file_node)
    exit (-1);
  return file_node->is_dir;
}

static int
inumber (int fd)
{
  struct list *file_list = &thread_current ()->file_list;
  struct file_node *file_node = get_file_node_by_fd (file_list, fd);
  if (!file_node)
    exit (-1);
  if (file_node->is_dir)
    return inode_get_inumber (dir_get_inode (file_node->dir));
  else
    return inode_get_inumber (file_get_inode (file_node->file));
}

static int
readdir (int fd, char *name)
{
  struct list *file_list = &thread_current ()->file_list;
  struct file_node *file_node = get_file_node_by_fd (file_list, fd);
  if (!file_node)
    exit (-1);
  if (!file_node->is_dir)
    exit (-1);
  check_buffer_put ((uint8_t *)name, NAME_MAX + 1);
  return dir_readdir (file_node->dir, name);
}

static int
mkdir (const char *dir)
{
  if (!check_string (dir))
    exit (-1);
  // printf ("mkdir syscall: %s\n", dir);
  return filesys_mkdir (dir);
}

static int
chdir (const char *dir)
{
  if (!check_string (dir))
    exit (-1);
  return filesys_chdir (dir);
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  // printf ("system call!\n");
  // printf("esp%p\n", f->esp);

  if (!check_int_get (f->esp))
    exit (-1);
  int syscall_id = *(int *)f->esp;
  if (syscall_id < 0 || syscall_id >= 30)
    exit (-1);

  void *func = syscall_func[syscall_id];
  if (func == NULL)
    exit (-1);
  int arg_number = syscall_arg_number[syscall_id];
  ASSERT (0 <= arg_number && arg_number <= 3);

  int args[3];
  for (int i = 0; i < arg_number; i++)
    {
      int *arg = (int *)f->esp + 1 + i;
      if (!check_int_get (arg))
        exit (-1);
      args[i] = *arg;
    }

  int return_value;
  if (arg_number == 0)
    return_value = ((int (*) (void))func) ();
  else if (arg_number == 1)
    return_value = ((int (*) (int))func) (args[0]);
  else if (arg_number == 2)
    return_value = ((int (*) (int, int))func) (args[0], args[1]);
  else if (arg_number == 3)
    return_value = ((int (*) (int, int, int))func) (args[0], args[1], args[2]);
  else
    NOT_REACHED ();

  f->eax = return_value;
  // printf ("end syscall\n");
}

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  syscall_arg_number[SYS_HALT] = 0;
  syscall_func[SYS_HALT] = (void *)halt;

  syscall_arg_number[SYS_EXIT] = 1;
  syscall_func[SYS_EXIT] = (void *)exit;

  syscall_arg_number[SYS_EXEC] = 1;
  syscall_func[SYS_EXEC] = (void *)exec;

  syscall_arg_number[SYS_WAIT] = 1;
  syscall_func[SYS_WAIT] = (void *)wait;

  syscall_arg_number[SYS_CREATE] = 2;
  syscall_func[SYS_CREATE] = (void *)create;

  syscall_arg_number[SYS_REMOVE] = 1;
  syscall_func[SYS_REMOVE] = (void *)remove;

  syscall_arg_number[SYS_OPEN] = 1;
  syscall_func[SYS_OPEN] = (void *)open;

  syscall_arg_number[SYS_FILESIZE] = 1;
  syscall_func[SYS_FILESIZE] = (void *)filesize;

  syscall_arg_number[SYS_READ] = 3;
  syscall_func[SYS_READ] = (void *)read;

  syscall_arg_number[SYS_WRITE] = 3;
  syscall_func[SYS_WRITE] = (void *)write;

  syscall_arg_number[SYS_SEEK] = 2;
  syscall_func[SYS_SEEK] = (void *)seek;

  syscall_arg_number[SYS_TELL] = 1;
  syscall_func[SYS_TELL] = (void *)tell;

  syscall_arg_number[SYS_CLOSE] = 1;
  syscall_func[SYS_CLOSE] = (void *)close;

  syscall_arg_number[SYS_MMAP] = 2;
  syscall_arg_number[SYS_MUNMAP] = 1;

  syscall_arg_number[SYS_CHDIR] = 1;
  syscall_func[SYS_CHDIR] = (void *)chdir;

  syscall_arg_number[SYS_MKDIR] = 1;
  syscall_func[SYS_MKDIR] = (void *)mkdir;

  syscall_arg_number[SYS_READDIR] = 2;
  syscall_func[SYS_READDIR] = (void *)readdir;

  syscall_arg_number[SYS_ISDIR] = 1;
  syscall_func[SYS_ISDIR] = (void *)isdir;

  syscall_arg_number[SYS_INUMBER] = 1;
  syscall_func[SYS_INUMBER] = (void *)inumber;
}

static struct file_node *
get_file_node_by_fd (struct list *file_list, int fd)
{
  struct list_elem *e;
  struct file_node *f;
  for (e = list_begin (file_list); e != list_end (file_list);
       e = list_next (e))
    {
      f = list_entry (e, struct file_node, elem);
      if (f->fd == fd)
        return f;
    }
  return NULL;
}
