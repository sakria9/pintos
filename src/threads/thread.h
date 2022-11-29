#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include "threads/synch.h"
#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "hash.h"
#include "malloc.h"
#include "stdlib.h"
#include "userprog/syscall.h"

/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
  };

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Priority. */
    struct list_elem allelem;           /* List element for all threads list. */

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
    int exit_status;                    /* Exit status of the thread. */
    struct list file_list;              /* List of files opened by the thread. */
    uint32_t next_fd;                   /* Next file descriptor to be assigned. */

    struct list child_list;             /* List of child processes. Node type is pa_ch_link */
    struct pa_ch_link *pa_link;
    struct file* exec_file;
#endif

#ifdef VM
    struct hash page_table; // Supplemental page table
    void *esp; // User stack pointer. Saved when interrupt occurs.
    struct list mmap_list; // List of mmaped files
    mapid_t next_mapid;
#endif

    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */
  };

/* A structure linking between parent process and child process
   It has its own life-cycle, to ensure that it will be destoryed after parent and children process are both dead.
   So that parent and child can visit it at any time they alive.
*/
struct pa_ch_link
{
  struct lock lock; // lock this structure.
  int reference_cnt; // reference counter. the structure will be deleted when count=0
  struct list_elem child_list_elem;  /* The list element of child_list*/
  struct thread* parent; 
  struct thread* child;
  int child_tid;
  struct semaphore child_dead;         /* Semaphore to check if child prcess is dead.  0: alive; 1:dead*/
  struct semaphore child_start; /* Semaphore to check if child finishes starting.*/
  int exit_code; // child process exit code. Used by process_wait.
  bool success; // Whether child process loaded successfully.
};

/* When a process exited, unlink the relationship with its parent or children
   You should call lock_aquire(&link->lock); before call this.
   Then you should't call lock_release(&link->lock);
*/
static void process_unlink(struct pa_ch_link *link)
{
  link->reference_cnt--;
  if (link->reference_cnt == 0)
  {
    free(link);
  } else {
    lock_release(&link->lock);
  }
}

/* Element of file_list in thread */
struct file_node
  {
    int fd;                             /* File descriptor. */
    struct file *file;                  /* File. */
    struct list_elem elem;              /* List element. */
  };

/* Element of mmap_list in thread */
struct mmap_node
  {
    mapid_t mapid;                      /* Mmap id. */
    struct file *file;                  /* File. */
    void *addr;                         /* Mapped address. */
    struct list_elem elem;              /* List element. */
  };

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

#endif /* threads/thread.h */
