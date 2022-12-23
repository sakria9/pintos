---
title: "PROJECT 4: FILE SYSTEMS\\newline DESIGN DOCUMENT"
author:
- Zhang Yichi <zhangych6@shanghaitech.edu.cn>
- Hu Aibo <huab@shanghaitech.edu.cn>
header-includes:
- \usepackage{tcolorbox}
- \newtcolorbox{myquote}{colback=red!5!white, colframe=red!75!black}
- \renewenvironment{quote}{\par\begin{myquote}}{\end{myquote}}
---

## PRELIMINARIES

> If you have any preliminary comments on your submission, notes for the
> TAs, or extra credit, please give them here.

> Please cite any offline or online sources you consulted while
> preparing your submission, other than the Pintos documentation, course
> text, lecture notes, and course staff.

## INDEXED AND EXTENSIBLE FILES

### DATA STRUCTURES

> A1: Copy here the declaration of each new or changed `struct` or
> `struct` member, global or static variable, `typedef`, or
> enumeration.  Identify the purpose of each in 25 words or less.

> A2: What is the maximum size of a file supported by your inode
> structure?  Show your work.

### SYNCHRONIZATION

> A3: Explain how your code avoids a race if two processes attempt to
> extend a file at the same time.

> A4: Suppose processes A and B both have file F open, both
> positioned at end-of-file.  If A reads and B writes F at the same
> time, A may read all, part, or none of what B writes.  However, A
> may not read data other than what B writes, e.g. if B writes
> nonzero data, A is not allowed to see all zeros.  Explain how your
> code avoids this race.

> A5: Explain how your synchronization design provides "fairness".
> File access is "fair" if readers cannot indefinitely block writers
> or vice versa.  That is, many processes reading from a file cannot
> prevent forever another process from writing the file, and many
> processes writing to a file cannot prevent another process forever
> from reading the file.

### RATIONALE

> A6: Is your inode structure a multilevel index?  If so, why did you
> choose this particular combination of direct, indirect, and doubly
> indirect blocks?  If not, why did you choose an alternative inode
> structure, and what advantages and disadvantages does your
> structure have, compared to a multilevel index?

## SUBDIRECTORIES

### DATA STRUCTURES

> B1: Copy here the declaration of each new or changed `struct` or
> `struct` member, global or static variable, `typedef`, or
> enumeration.  Identify the purpose of each in 25 words or less.

```cpp
struct inode_disk
{
    // ignore irrelevant members
    unsigned is_dir; /* 1 if directory, 0 if file. */
}

/* Element of file_list in thread */
struct file_node
{
  // ignore irrelevant members
  bool is_dir;
  union
  {
    struct file *file; /* File. */
    struct dir *dir;
  };
};

struct thread
{
    // ignore irrelevant members
    struct dir *cwd; /* Current Working Directory */
}
```

### ALGORITHMS

> B2: Describe your code for traversing a user-specified path.  How
> do traversals of absolute and relative paths differ?

We first split the basename and the parent path.
If the parent path is empty, then the parent path is cwd.
If the parent path begins with '/', then the parent path is absolute path.
If the parent path does not begin with '/', then the parent path is relative path.
If it is a relative path, the begining of the traversal is `thread_current()->cwd`.
If it is a absolute path, the begining of the traversal is `dir_open_root()`.


### SYNCHRONIZATION

> B4: How do you prevent races on directory entries?  For example,
> only one of two simultaneous attempts to remove a single file
> should succeed, as should only one of two simultaneous attempts to
> create a file with the same name, and so on.

The whole filesystem module is single thread by using a lock.
Therefore, there is no "race" because every file operation in one by one.
No two file operations would execute simultaneously.

> B5: Does your implementation allow a directory to be removed if it
> is open by a process or if it is in use as a process's current
> working directory?  If so, what happens to that process's future
> file system operations?  If not, how do you prevent it?

No. We check whether the directory is open or not before removing it.
There is a `open_cnt` in the inode structure.
If it equals to 1, then we can remove it.

### RATIONALE

> B6: Explain why you chose to represent the current directory of a
> process the way you did.

We add `struct dir *cwd;` in the `struct thread` structure.
Because our implementation does not allow a directory to be removed if it is open by a process or if it is in use as a process's current working directory.
So it is nice to have a pointer to the current directory of a process, which prevents the directory from being removed.
And it is more efficient to use a pointer than to open the directory every time we need it.

## BUFFER CACHE

### DATA STRUCTURES

> C1: Copy here the declaration of each new or changed `struct` or
> `struct` member, global or static variable, `typedef`, or
> enumeration.  Identify the purpose of each in 25 words or less.

### ALGORITHMS

> C2: Describe how your cache replacement algorithm chooses a cache
> block to evict.

> C3: Describe your implementation of write-behind.

> C4: Describe your implementation of read-ahead.

### SYNCHRONIZATION

> C5: When one process is actively reading or writing data in a
> buffer cache block, how are other processes prevented from evicting
> that block?

> C6: During the eviction of a block from the cache, how are other
> processes prevented from attempting to access the block?

### RATIONALE

> C7: Describe a file workload likely to benefit from buffer caching,
> and workloads likely to benefit from read-ahead and write-behind.
