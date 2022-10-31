#include "userprog/syscall.h"
#include "devices/shutdown.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include <stdio.h>
#include <syscall-nr.h>

typedef int pid_t;

static void syscall_handler (struct intr_frame *);

int syscall_arg_number[30];
void *syscall_func[30];

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
}
static int
wait (pid_t pid)
{
}
static bool
create (const char *file, unsigned initial_size)
{
}
static bool
remove (const char *file)
{
}
static int
open (const char *file)
{
}
static int
filesize (int fd)
{
}
static int
read (int fd, void *buffer, unsigned size)
{
}
static int
write (int fd, const void *buffer, unsigned size)
{
  // TODO: check buffer address valid
  if (fd == 1)
    {
      putbuf (buffer, size);
      return size;
    }
  else
    {
      printf ("NOT IMPL\n");
      exit (-1);
    }
}
static void
seek (int fd, unsigned position)
{
}
static unsigned
tell (int fd)
{
}
static void
close (int fd)
{
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  // get the system call number
  printf ("system call!\n");
  // printf("esp%p\n", f->esp);

  // TODO: check esp address
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
      // check esp + 4*(i+1)
      args[i] = *(int *)(f->esp + 4 * (i + 1));
    }

  int return_value;
  if (arg_number == 0)
    {
      return_value = ((int (*) (void))func) ();
    }
  else if (arg_number == 1)
    {
      return_value = ((int (*) (int))func) (args[0]);
    }
  else if (arg_number == 2)
    {
      return_value = ((int (*) (int, int))func) (args[0], args[1]);
    }
  else if (arg_number == 3)
    {
      return_value
          = ((int (*) (int, int, int))func) (args[0], args[1], args[2]);
    }
  else
    {
      NOT_REACHED ();
    }

  f->eax = return_value;
  printf ("end syscall\n");
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
  syscall_func[SYS_CLOSE] = (void *)SYS_CLOSE;

  syscall_arg_number[SYS_MMAP] = 2;
  syscall_arg_number[SYS_MUNMAP] = 1;
  syscall_arg_number[SYS_CHDIR] = 1;
  syscall_arg_number[SYS_MKDIR] = 1;
  syscall_arg_number[SYS_READDIR] = 2;
  syscall_arg_number[SYS_ISDIR] = 1;
  syscall_arg_number[SYS_INUMBER] = 1;
}
