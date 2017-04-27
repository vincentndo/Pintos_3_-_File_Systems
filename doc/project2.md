Design Document for Project 2: User Programs
============================================

## Group Members

* David Fang <fangdavid@berkeley.edu>
* Derek Hsiao <hsiaoderek@berkeley.edu>
* Ninh Do <ninhdo@berkeley.edu>
* Robert Matych <rmatych@berkeley.edu>

## Task 1: Argument Passing

### Data structures and functions

1. `argv` -- We have an argv array that is created by tokenizing a line of a command line string
2. We plan on creating a function called `get_num_args` which will run an initial pass on the command line string with `strtok_r` to figure out how big the argv array will be.
3. We also plan on implementing a function responsible for populating the argv array as well as the stack called `populate_stack_and_argv`. This function will use `strtok_r` to break the command line string into its individual, space-separated tokens. We first add these tokens onto the stack one-by-one while decrementing the stack pointer for each token. At each push onto the stack, we populate the corresponding `argv` index with its address on the stack. 
4. A third function called `pad_stack` will be implemented to decrement the stack pointer such that it is word-aligned. 
5. A fourth function called `write_arr_to_stack` will `memcpy` the `argv` results from the `populate_stack_and_argv` call onto the stack. Finally, it will add a pointer to `argv[0]` and set the value of `argc`. Additionally, we will populate the return address on the stack frame to be a **dummy** null pointer.


### Algorithms

There are two functions that will parse through the given command line string. Our parsing method will resemble something of the following:

```c
char* token, save_ptr;
for (token = strtok_r (command_line_str, " ", &save_ptr); token != NULL;
                       token = strtok_r (NULL, " ", &save_ptr))
    {
        /* logic for get_num_args() and populate_stack_and_argv() here */
    }
```

### Synchronization

There are no synchronization issues with passing in command line string to the child thread.

### Rationale
We choose to split up the string within `start_process` because it's closest to the point at which the individual tokens are used- writing them to the stack.


## Task 2: Process Control Syscalls

### Data structures and functions

#### Thread geneology

In order to the keep the parent and child threads connected, we will be including a struct located in kernel memory that maintains data and synchronization primitives that allow the parent and child threads to interact upon `exec` and `wait` syscalls:

```c
struct thread_kinship
{
    char* cmd_line_input;
    struct semaphore kin_sema;
    pid_t child_pid;
    int exit_status;
    list_elem child_elem;
}
```

Furthermore, we will be adding the following fields to the `thread` struct.

```c
...
struct list child_list;
struct thread_kinship*  kin_struct;
...
```

In order for a parent thread to be able to maintain a reference to **all** of its children, we will be adding a list to the `thread` struct called `child_list`, which will be a list of `thread_kinship` structs. Similarly, we want the child thread to be able to access the shared information with its parent as well. We will be adding `kin_struct` for this purpose.

#### Function summaries

1. `halt_syscall` -- Terminates Pintos by simply calling the `shutdown_power_off` function.
2. `exit_syscall` -- Calls `free_thread_resources`, sets the `eax` register to the exit status code, prints the exit message, and calls `thread_exit`.
3. `practice_syscall` -- Given an `int` input, increments it by one and puts it in the `eax` register.
4. `exec_syscall` -- The `exec` syscall will call `process_execute`, passing in the command line inputs.
5. `wait_syscall` -- Checks that the pid corresponds to one of the current thread's children. If so, waits for the child thread with `pid` to exit before continuing by using utilizing the `kin_sema` semaphore.
6. `free_thread_resources` -- Frees all of a thread's dynamically allocated data and releases all shared synchronization primitives that the thread currently holds.

### Algorithms

#### `exec`

When a user programs calls `exec`, we `malloc` space in kernel memory for a `thread_kinship` struct. We then populate it with a semaphore initialized to 0 and a pointer to the command line string (which exists in kernel memory). The semaphore serves to prevent the parent from accessing the `thread_kinship` struct before the child process finishes populating it. By using a semaphore, we are able to effectively block the parent process from continuing until the child process has finished attempting to load the executable. From here we have two possibilities:

1. **The load succeeds**, in which case the child thread sets the `child_pid` inside of its respective `thread_kinship` struct to be the thread's `tid` value.
2. **The load fails**, in which case the child thread sets the `child_pid` inside of its `thread_kinship` struct to be -1.

After the `child_pid` is set, the child will call `sema_up` on the shared semaphore inside the `thread_kinship` struct and unblock the parent. To check for load success, the parent can now `sema_down` to access the struct and check to see if the `child_pid` is -1. We copy the `child_pid` to a local variable, which will be our return value. But before returning, one of the following should occur:

1. If `child_pid` is -1, the load must have failed, in which case the child thread calls `thread_exit`. The `exec` failed to complete, so there is no point in saving any reference to that child process. Thus, we are free to **free** the `thread_kinship` struct and other dynamically allocated data.
2. If the `child_pid` is a nonnegative value, the load must have succeeded, in which case we want to add the `thread_kinship` struct to the parent's `child_list`.

At the end of our `exec`, we will return the local copy of `child_pid`.

#### `wait`

We start our wait syscall by iterating through the current thread's `child_list` to see if the given `pid` actually corresponds to one of the current thread's direct children. If not, we immediately return -1. 

If a child thread with `pid` exists, the calling (parent) thread will decrement the `kin_sema` semaphore once, causing it to block itself until the child thread terminates, either normally or via a forced kernel termination. In order to prevent deadlock from occuring in the event that the child thread is forcibly terminated, we need to ensure that `sema_up` will be called at some point to unblock the parent thread. This can be achieved by incrementing the `kin_sema` value in `process_exit`, which occurs in any time of thread termination.

Once a parent thread successfully decrements the `kin_sema` value, it means a wait has completed. In this case, the child process must have terminated. Upon termination, the child thread will write its exit code onto its `thread_kinship` struct for the parent thread to use when it calls wait. Additionally, a successful wait implies that the child thread has exited. This child process cannot be waited on again, so we remove it from the parent's `child_list` and free all dynamically allocated data associated with it. In order to return the exit status, before freeing we make a local copy of it from the `thread_kinship` struct. Finally, we return the exit status.

### Synchronization
Every parent-child thread pair has an associated semaphore in a shared `thread_kinship` struct. The semaphore is used to complete the `exec` and `wait` syscalls. In the case of `exec`, the semaphore is used to prevent the syscall from returning until the child process has completed its attempt to load (when a `pid` can be returned). In the case of `wait`, the semaphore is used to make sure that the calling thread is blocked only when the child process has not yet terminated. The semaphore also prevents concurrent accesses to the `exit_status` and `pid` members of the `thread_kinship` struct.

### Rationale
We noticed a similar and overlapping synchronization requirement in both the `exec` and `wait` syscalls, so we decided to create the `thread_kinship` struct to meet this requirement in the cleanest way possible and with the least instances of duplicate code. Ideologically, a `wait` cannot occur until a successful `exec` occurs, so these syscalls in many ways are 2 sides of the same coin.

We decided to allocate the `thread_kinship` structs dynamically because we couldn't reasonably impose a limit on how many children a process can have. By using `malloc ()` and the kernel memory pool, the maximum number of child processes is not limited by the size of the kernel stack pointer. Dis is good, ya?

## Task 3: File Operation Syscalls

### Data structures and functions
Every open file will have an associated `file_stats` struct:
```c
struct file_stats
{
    bool pending_removal;
    unsigned ref_count;
    struct list_elem file_stats_elem;
}
```
Every open file also has a file descriptor `fd`. The `file_blob` struct serves to map a process-specific `fd` to an open `file *` as well as an associated `file_stats` struct:
```c
struct file_blob
{
    int fd;
    file *file;
    struct file_stats *stats;
    struct list_elem file_elem;
}
```




1. `check_addr_validity` -- Checks whether a pointer references null, kernel space, or unmapped addresses. Returns true if the pointer references a valid address. Else, return false if it references any of the three aforementioned types.
2. `create_syscall` -- Returns the result of a call to `filesys_create`.
3. `remove_syscall` -- "Removes" a file. If the file was open on a call to remove, simply set the `pending_removal` boolean in its associated `file_stats` struct. Otherwise, call `filesys_remove`.
4. `open_syscall` -- Opens a file using `filesys_open`. Initializes a new `file_stats` struct if one is not found in the global list, or increments the existing `file_stats -> ref_count` value. If the file is pending removal, the syscall fails and returns -1.
5. `filesize_syscall`-- Returns the result of a call to `file_length`.
6. `read_syscall` -- First checks the validity of the full input buffer using check_addr_validity. Reads `size` bytes from the input file descriptor into the buffer. Returns the number of bytes successfully added to the buffer.
7. `write_syscall` -- Checks validity of the buffer, and writes the contents of the buffer to the output `fd`. Returns the number of bytes successfully written to `fd`.
8. `seek_syscall` -- Returns the result of a call to `file_seek`. Additionally, checks that the offset is less than the size of the file.
9. `tell_syscall` -- Returns the result of a call to `file_tell`.
10. `close_syscall` -- If the `fd` larger than 1, calls `file_close` and decrements the `ref_count` field of the file's associated `file_stats` struct. If the count is `0` and it's pending removal, then call `filesys_remove` on the file. Furthermore, remove the `file_stats` struct from the global list of open files. Finally, remove the `file_blob` associated with the input `fd` from the process's `file_blob` list and then frees the `file_blob`.

### Algorithms
#### `open`
First, check the global file list for a `file_stats` struct associated with the input file name.
If it is found in the list and pending removal, return -1 -- the syscall should fail. If it is **not** found in the list, create a new `file_stats` struct and insert it into the global list.

At this point, we create a `file_blob` for the file that references the existing/newly created `file_stats` struct. Adds the `file_blob` to the calling process's list of open files.

Finally, increment the `file_stats -> ref_count` value by 1.
#### `close`
If `fd` is less than 2, or it is an invalid file descriptor, then the syscall fails silently. Return.

If the `fd` is found within calling process's list of open files,
1. Call `file_close` on `file_blob -> file`.
2. Decrement the `file_stats -> ref_count` value by 1.
3. If `ref_count` is 0 and the file is also pending removal, call `filesys_remove`. Additionally, remove corresponding `file_stats` struct from the global list and free it.
4. Finally, free the `file_blob` in the process's own list of file handles.


#### `remove`
There are two main cases:
1. If the file is not in the global file list, it is not being referenced by any running processes. It should just be deleted from the file system, so return the result of `filesys_remove`.
2. If the file _is_ in the global file list, set the `file_stats -> pending_removal` to true.  Returns true.

#### `Checking address validity`
Our `check_addr_validity` will resemble somewhat of the following. In order to check that our buffer is valid, we will be using this function on all of the pages that the buffer memory traverses to make sure we are not accessing invalid memory.

```c
bool check_addr_validity (void *ptr)
{
    if (ptr == NULL || is_kernel_vaddr (ptr) ||
        pagedir_get_page(thread_current() -> pagedir, ptr) == NULL) 
        {
            return false;
        }
    return true;
}
```

### Synchronization
We need to make sure that only one function from `file.c` and `filesys.c` can be called at a time to prevent concurrent file accesses. Any call to any function in either of these files must be surrounded by a lock acquire and release. We create a global `file_syscall_lock` inside `syscall.c` to accomplish this amazing feat of synchronization.

### Rationale
We decided to create 2 different types of lists to represent the different "views" of open files:

Every process needs to be able to map a file descriptor to a `file *` struct. A process's file descriptors are unique to itself, so every process should keep a list of mappings from `fd` values to the corresponding `file *` struct.

A global view of the open files, on the other hand, serves as a way to map a file's name to the number of file handles that are related to it. This view of the files is useful for the `remove` syscall, since the behavior is different depending on how many processes are using the file in question.

## Design Document Additional Questions

1. The test `sc-bad-sp.c` is an example of a syscall invoked with an invalid stack pointer. On line 18, an assembly instruction moves `-(64*1024*1024)` into `$esp`. Then it invokes a system call. The value stored in `$esp` is much larger than `PHYS_BASE` (compare 64 Gb to the 4 Gb that Pintos is allowed). Since dereferencing `$esp` will result in a larger value than PHYS_BASE, a correct implementation will kill the calling thread.

2. The test `sc-bad-arg.c` uses a valid stack pointer which is too close to PHYS_BASE. In line 14, there are 3 assembly instructions.
    ```c
    1. `movl $0xbffffffc, %%esp;` //sets the stack pointer to 4 bytes below the PHYS_BASE. 
    2. `movl %0, (%%esp);` //sets the 4 bytes above the stack pointer to 0 as argument to syscall
    3. `int $0x30" : : "i" (SYS_EXIT));` // requests a SYCALL with i = SYS_EXIT
    ```

    Instruction 3 in sc-bad-arg.c triggers syscall_handler in line 16 of syscall.c. `syscall_handler` will attempt to access `args[0]` to get the syscall number. However, `args[0]` would be above the `PHYS_BASE`, which would be an illegal memory access.

3. The current test suite has no tests that check for any file remove syscalls. In order to provide enough coverage for this topic, we will need to check (1) general removal when no threads reference a file should just remove the read, (2) pending removal boolean is actually set when removal is called while threads are still referencing it. For the second case, we also want to make sure that no further threads can open that file descriptor. Furthermore, threads that have the file descriptor open should still be able to write changes and read from the file; we should check that this is so.

## GDB Questions

1. The **address** of the thread is 0xc00e00. The thread's **name** is "main."

    The thread structs of all the threads that were present in Pintos at the time of our breakpoint include:
    
    ```
    pintos-debug: dumplist #0: 0xc000e000
    
    {tid = 1, status = THREAD_RUNNING, name = "main", '\000' <repeats 11 times>, stack = 0xc000ee0c "\210", <incomplete sequence \357>, priority = 31, allelem = {prev = 0xc0034b50 <all_list>, next = 0xc0104020}, elem = {prev = 0xc0034b60 <ready_list>, next = 0xc0034b68 <ready_list+8>}, pagedir = 0x0, magic = 3446325067}

    pintos-debug: dumplist #1: 0xc0104000

    {tid = 2, status = THREAD_BLOCKED, name = "idle", '\000' <repeats 11 times>, stack = 0xc0104f34 "", priority = 0, allelem = {prev = 0xc000e020, next = 0xc0034b58 <all_list+8>}, elem = {prev = 0xc0034b60 <ready_list>, next = 0xc0034b68 <ready_list+8>}, pagedir = 0x0, magic = 3446325067}
    ```

2. The backtrace leading up to the current breakpoint is as follows:

    ```
    #0  process_execute (file_name=file_name@entry=0xc0007d50 "args-none") at ../../userprog/process.c:32
    #1  0xc002025e in run_task (argv=0xc0034a0c <argv+12>) at ../../threads/init.c:288
    #2  0xc00208e4 in run_actions (argv=0xc0034a0c <argv+12>) at ../../threads/init.c:340
    #3  main () at ../../threads/init.c:133
    ```

3. The current thread has the **address** 0xc010a000 and the **name** "args-none." The list of threads that exist in Pintos at this point in time are as follows:

    ```
    pintos-debug: dumplist #0: 0xc000e000 
    
    {tid = 1, status = THREAD_BLOCKED, name = "main", '\000' <repeats 11 times>, stack = 0xc000eebc "\001", priority = 31, allelem = {prev = 0xc0034b50 <all_list>, next = 0xc0104020}, elem = {prev = 0xc0036554 <temporary+4>, next = 0xc003655c <temporary+12>}, page dir = 0x0, magic = 3446325067}

    pintos-debug: dumplist #1: 0xc0104000

    {tid = 2, status = THREAD_BLOCKED, name = "idle", '\000' <repeats 11 times>, stack = 0xc0104f34 "", priority = 0, allelem = {prev = 0xc000e020, next = 0xc010a020}, elem = {prev = 0xc0034b60 <ready_list>, next = 0xc0034b68 <ready_list+8>}, pagedir = 0x0, magic = 3446325067}

    pintos-debug: dumplist #2: 0xc010a000

    {tid = 3, status = THREAD_RUNNING, name = "args-none\000\000\000\000\000\000", stack = 0xc010afd4 "", priority = 31, allelem = {prev = 0xc0104020, next = 0xc0034b58 <all_list+8>}, elem = {prev = 0xc0034b60 <ready_list>, next = 0xc0034b68 <ready_list+8>}, pagedir = 0x0, magic = 3446325067}
    ```

4. The thread running `start_process` was created in the `process_execute` function, line 45:

    ```
    tid = thread_create (file_name, PRI_DEFAULT, start_process, fn_copy);
    ```

5. The line of the user program that caused the page fault has hex address 0x0804870c.

6. After loading the user symbols, `btpagefault` gives us ...

    ```
    _start (argc=<error reading variable: can't compute CFA for this frame>, argv=<error reading variable: can't compute CFA for this frame>) at ../../lib/user/entry.c:9
    ```
    
7. The user page faults at this line because we have not yet implemented any argument passing functionality yet, so it is unable to read any `argc` or `argv` values.
