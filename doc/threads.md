            +--------------------+
            |        CS 140      |
            | PROJECT 1: THREADS |
            |   DESIGN DOCUMENT  |
            +--------------------+

#### GROUP 

> Fill in the names and email addresses of your group members.

Zhang Yichi <zhangych6@shanghaitech.edu.cn>

Hu Aibo <huab@shanghaitech.edu.cn>


#### PRELIMINARIES 

> If you have any preliminary comments on your submission, notes for the
> TAs, or extra credit, please give them here.

> Please cite any offline or online sources you consulted while
> preparing your submission, other than the Pintos documentation, course
> text, lecture notes, and course staff.

                 ALARM CLOCK
                 ===========

#### DATA STRUCTURES 

> A1: Copy here the declaration of each new or changed `struct' or
> `struct' member, global or static variable, `typedef', or
> enumeration.  Identify the purpose of each in 25 words or less.

```cpp
/* List for all sleeping threads. 
   Sort by awake_time in ascending order.
*/
static struct list sleep_list;
struct thread
  {
    /* For sleeping thread. 
       Tick to wake up. 
       If awake_tick < 0, the thread is not sleeping */
    int awake_tick;
    struct list_elem sleep_elem;        /* List element for sleep list. */
  };

```

#### ALGORITHMS 

> A2: Briefly describe what happens in a call to timer_sleep(),
> including the effects of the timer interrupt handler.

timer_sleep calls thread_sleep

thread_sleep disables interrupt immediately, and enables interrupt after it is done.

Then it inserts the thread into sleep_list, marks the thread as sleeping, and block the thread.

When the timer interrupt handler is called, it will check the sleep_list, and wake up all the threads that should be woken up.

> A3: What steps are taken to minimize the amount of time spent in
> the timer interrupt handler?

thread_sleep maintains a sorted sleep_list, so that in the timer interrupt handler, it can wake up threads in O(k) time, where k is the number of threads that should be woken up at that tick.

#### SYNCHRONIZATION 

> A4: How are race conditions avoided when multiple threads call
> timer_sleep() simultaneously?

When step into thread_sleep, it disables interrupt immediately, so that the timer interrupt handler will not be called. Then thread schedule will not happen. So no two threads will change sleep_list at the same time.

> A5: How are race conditions avoided when a timer interrupt occurs
> during a call to timer_sleep()?

When step into thread_sleep, it disables interrupt immediately, so that the timer interrupt will not be called.


#### RATIONALE 

> A6: Why did you choose this design?  In what ways is it superior to
> another design you considered?

We choose to maintain a sorted sleep_list, it is simple and efficient. It takes O(1) when pop a thread from the list in time interrupt handler, so that it will not take too much time in the timer interrupt handler. In timer_sleep, it will take at worst O(n) time, but in practice, threads that call sleep later are usually woken up later as well, so the O(n) upper bound is not easily reached.

We have thought about maintaining an unordered list. But it will take O(n) time in timer interrupt handler.

We also thought about maintaining a heap, but the heap design is relatively complex, and the test data size of pintos is not very large, so the O(logn) time efficiency advantage is not obvious. If there will be some large testcase in later project, we may consider to design a heap for alarm clock.


             PRIORITY SCHEDULING
             ===================

#### DATA STRUCTURES 

> B1: Copy here the declaration of each new or changed `struct' or
> `struct' member, global or static variable, `typedef', or
> enumeration.  Identify the purpose of each in 25 words or less.

> B2: Explain the data structure used to track priority donation.
> Use ASCII art to diagram a nested donation.  (Alternately, submit a
> .png file.)

```cpp
// src/thread/thread.h
struct thread
  {
    // ...
    int priority;                       /* Priority after donations. */
    int raw_priority;                   /* Priority before donations. */
    struct list donator_list;           /* Threads that donating this thread */
    struct thread* donatee;             /* Thread that this thread is donating */
    struct list_elem donatee_elem;      /* List element for donator_list */
    // ...
  };

// src/thread/synch.c
struct semaphore_elem 
  {
    struct list_elem elem;              /* List element. */
    struct semaphore semaphore;         /* This semaphore. */
    struct thread* thread;              /* Thread that is waiting this semaphore */
  };
```

#### ALGORITHMS 

> B3: How do you ensure that the highest priority thread waiting for
> a lock, semaphore, or condition variable wakes up first?

For semaphore, we iterate over `sema->waiters`, and get the thread with the highest priority by helper function `thread_list_pop_max_priority` `list_pop_max` `semaphore_elem_less`. And then we call `thread_unblock` to unblock the thread. Because the thread we unblocked may have higher priority than the current thread, we call `thread_yield` to yield the current thread.

For lock, we use `semaphore` to implement it. So we just need to call `sema_up (&lock->semaphore)`.

For condition variable,  we iterate over `cond->waiters`, and get the waiter with the highest priority by helper function `list_pop_max` `semaphore_elem_less`.

> B4: Describe the sequence of events when a call to lock_acquire()
> causes a priority donation.  How is nested donation handled?

1. Disable interrupt to avoid race condition.
2. Check lock holder. If it is not null, we need to donate priority. Otherwise, we just skip the donation. Otherwise, we just skip the donation.
3. Use a while loop, donate the priority to the holder, donate the priority to the donatee of the holder, donate the priority to the donatee of the the donatee of the holder, and so on. We use `thread->donatee` to track the donatee of the thread, and use `thread->donators` to track the direct donators of the thread.
4. Call `sema_down` to acquire the lock.
5. set `lock->holder` to the current thread.
6. Set interrupt status to the original status.

> B5: Describe the sequence of events when lock_release() is called
> on a lock that a higher-priority thread is waiting for.

1. Disable interrupt to avoid race condition.
2. Clear donators of the current thread.
3. Update the priority of the current thread to its raw priority.
4. Set `lock->holder` to null.
5. `sema_up (&lock->semaphore)` to unblock the thread with the highest priority.
6. Set interrupt status to the original status.
7. Call `thread_yield` to yield the current thread. 

#### SYNCHRONIZATION 

> B6: Describe a potential race in thread_set_priority() and explain
> how your implementation avoids it.  Can you use a lock to avoid
> this race?

`thread_set_priority()` uses `donator_list` to re-calculate priority.
`donator_list` is only modified in `lock_release` and `lock_acquire`.
We disable the interrupt in `lock_release` and `lock_acquire`.
So we avoid the potential race.

#### RATIONALE 

> B7: Why did you choose this design?  In what ways is it superior to
> another design you considered?

1. To handle `thread_set_priority` correctly, we have to track the donators of the thread. So we add `donator_list` and `donatee_elem` to the thread struct.
2. To handle chain donation, we add a `donatee` to the thread struct to track the donatee of the thread.
3. While changing the list to a heap reduces the time complexity, it makes the code more complex. So we use list to implement the priority queue.

              ADVANCED SCHEDULER
              ==================

#### DATA STRUCTURES 

> C1: Copy here the declaration of each new or changed `struct' or
> `struct' member, global or static variable, `typedef', or
> enumeration.  Identify the purpose of each in 25 words or less.

```cpp
typedef int fp32; // 17.14 Fix-Point Real Number
fp32 load_avg; // load_avg defined by 4.4BSD scheduler
struct thread
  {
    int nice;                           /* The niceness defined by 4.4BSD scheduler */
    fp32 recent_cpu;                    /* The recent cpu defined by 4.4BSD scheduler */
  };
```

#### ALGORITHMS 

> C2: Suppose threads A, B, and C have nice values 0, 1, and 2.  Each
> has a recent_cpu value of 0.  Fill in the table below showing the
> scheduling decision and the priority and recent_cpu values for each
> thread after each given number of timer ticks:

timer     recent_cpu    priority   
|ticks|   A|   B|   C|   A|   B|   C|thread to run|
|-----|  --|  --|  --|  --|  --|  --|---------|
| 0   |  0 |  0 |  0 | 63 | 61 | 59 |   A     |
| 4   | 4  |  0 |  0 | 62 | 61 | 59 |   A     | 
| 8   | 8  |  0 |  0 | 61 | 61 | 59 |   A     |
|12   | 12 |  0 |  0 | 60 | 61 | 59 |   B     |
|16   | 12 |  4 |  0 | 60 | 60 | 59 |   B     |
|20   | 12 |  8 |  0 | 60 | 59 | 59 |   A     |
|24   | 16 |  8 |  0 | 59 | 59 | 59 |   A     |
|28   | 20 |  8 |  0 | 58 | 59 | 59 |   B     |
|32   | 20 | 12 |  0 | 58 | 58 | 59 |   C     |
|36   | 20 | 12 |  4 | 58 | 58 | 58 |   C     |

> C3: Did any ambiguities in the scheduler specification make values
> in the table uncertain?  If so, what rule did you use to resolve
> them?  Does this match the behavior of your scheduler?

Yes, when there are two or more threads having the maximum priority, choose any one of them is ok. 

I choose the current running thread first if it has the maximum priority. Then the any one of the threads with the maximum priority.  

It may not match the behavior of my scheduler. My scheduler always choose the first thread in the ready list which has the maximum priority.

> C4: How is the way you divided the cost of scheduling between code
> inside and outside interrupt context likely to affect performance?



#### RATIONALE 

> C5: Briefly critique your design, pointing out advantages and
> disadvantages in your design choices.  If you were to have extra
> time to work on this part of the project, how might you choose to
> refine or improve your design?

My code is designed exactly as described in the documentation. 

I think the advantage of my design is that it is clear enough and easy to understand.

The disadvantage may be that the scheduler is not efficient enough. For example, on every 4 ticks, It updates the priority of all threads, and then scan the entire ready list for the thread with the highest priority.  If I have more time, it may be better to only update the threads whose nice and recent_cpu are changed, and use a heap to maintain the maximum priority thread. 

> C6: The assignment explains arithmetic for fixed-point math in
> detail, but it leaves it open to you to implement it.  Why did you
> decide to implement it the way you did?  If you created an
> abstraction layer for fixed-point math, that is, an abstract data
> type and/or a set of functions or macros to manipulate fixed-point
> numbers, why did you do so?  If not, why not?

I use ``typdef int fp32;`` to represent a 17.14 fixed-point real number. And use functions like ``fp32 fp32_create(int)``, ``fp32 fp32_add (fp32 x, fp32 y)`` to operate the fixed-point number.

Using a new type name to represent the fixed-point number is more clear for reader. 

Using functions can abstract the implementation of fixed-point number. So that coder can use the functions to operate the fixed-point number without knowing the implementation details.

               SURVEY QUESTIONS
               ================

Answering these questions is optional, but it will help us improve the
course in future quarters.  Feel free to tell us anything you
want--these questions are just to spur your thoughts.  You may also
choose to respond anonymously in the course evaluations at the end of
the quarter.

> In your opinion, was this assignment, or any one of the three problems
> in it, too easy or too hard?  Did it take too long or too little time?

> Did you find that working on a particular part of the assignment gave
> you greater insight into some aspect of OS design?

> Is there some particular fact or hint we should give students in
> future quarters to help them solve the problems?  Conversely, did you
> find any of our guidance to be misleading?

> Do you have any suggestions for the TAs to more effectively assist
> students, either for future quarters or the remaining projects?

> Any other comments?
