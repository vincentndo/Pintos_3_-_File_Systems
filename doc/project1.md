Design Document for Project 1: Threads
======================================

## Group Members

* David Fang <fangdavid@berkeley.edu>
* Derek Hsiao <hsiaoderek@berkeley.edu>
* Ninh Do <ninhdo@berkeley.edu>
* Robert Matych <rmatych@berkeley.edu>

## Task 1: Efficient Alarm Clock

### Data structures and functions

1. We will introduce a new member `wake_up_time` to the `thread` struct as follows:

    ```c
    struct thread
    {
        .../* existing members */
        int wake_up_time /* newly added */
    }; thread
    ```

2. We define a struct to wrap a thread struct so that it can be used in a list:

    ```c
    struct wrapped
    {
        struct thread *contents;
    }; wrapped
    ```

3. We define a function that extracts the wake_up_time from a thread:

    ```c
    int64_t get_wakeup_time(struct thread *t)
    {
        return t -> wake_up_time;
    }
    ```

### Algorithms

We will maintain an ordered linked list that holds the sleeping threads. This is the provided list in list.c.  The linked_list is checked when timer_interrupt is called.

#### Putting a thread to sleep
When `timer_sleep` is called, compute `wake_up_time`, which is `timer_ticks + ticks`. Then we set the `wake_up_time` of the current thread to this value. Next, wrap the current thread struct into a `wrapped` struct. The wrapped struct inserted into the list with `list_insert_ordered` in order to maintain an ordered list. Finally, call `curthread_block()` to block the thread.

#### Waking a thread up
When `timer_interrupt` is called, check whether 100 ticks have passed with `(ticks % 100) == 0)`. This is to reduce the frequency at which we check our list of sleeping threads. When the above condition is met, check whether the list of sleeping threads is empty. If it is, return. While the list is not empty, pop off threads whose `wake_up_time` values are greater than the current tick. Call `thread_unblock (thread *t)` on each thread that was popped off the list in the previous step.
     
### Synchronization

Inserting into the priority queue is not thread safe. That is, the order of the underlying linked list will be nondeterministic depending on the order that threads are added to it. Thus we must ensure that there are no race conditions. We solve this using locks: when a thread is about to sleep, it acquires a lock. After it is in the sleeping queue, the lock is released.

### Rationale

Our design is less busy than the original one. When a thread goes sleeping, CPU is taken by another thread so no time is wasted. We add some new functions to ensure that CPU is assigned to the next thread and the sleeping threads wake up on time. The coding is as above. The new issues arise when we have to implement a priority queue for sleeping threads and put a new sleeping thread into the queue, as well as popping one out whenever it wake up. With a decent design of priority queue, the structure takes O(n) in space and each action takes O(log n) time, where 'n' is the number of sleeping threads. 


## Task 2: Priority Scheduler

### Data structures and functions

To achieve priority scheduling, we will be introducing several new fields to the thread struct. 

1. `struct list_elem donor_list` -- Responsible for maintaining a list of the thread's potential donors for priority donation. This allows us to keep track of donors that may be donating for different locks that the thread may have in its possession. Maintaining a list of donors also makes it easier to transfer donations to the next thread to inherit donors when the current thread releases its lock.

2. `struct lock awaited_lock` -- Keeps a reference to the lock that thread is waiting for. For our design, we expect to put the priority donating functions within `thread.c`. However, in doing so, we lose reference to the thread that we want to donate to. Maintaining an `awaited_lock` variable lets us know what lock the current thread is waiting for and the lock's holder: the thread we want to donate to.

3. `int effective_priority` -- Acts as the thread's actual priority that dynamically updates based on priority donations. This field is initialized to be the same value as the default `priority` field in the thread structure. We keep the two values separated so that we are able to revert a thread's donated priority back to its original priority after the thread releases its lock.

We will also be adding a new function for priority donating in the `thread.c` file.

1. `void donate_priority (void)` -- Donates the current thread's priority to the thread that current holds the lock that the current thread is waiting for. *Full definition of* `donate_priority ()` _is found below in the **Algorithms** section_.

### Algorithms

#### Choosing the next thread to run
When scheduling the next thread to run for priority scheduling, we will schedule the thread that has the highest priority value in the ready queue (i.e. `ready_list`). In order to do this efficiently, we will make use of the ordering utility functions that the current linked list implementation provides. For example, we will use `list_insert_ordered ()` in order to keep the highest priority thread in the front of the ready queue. Using this scheme, we can use `list_pop_front ()` to get the thread with the highest priority when we call `schedule ()`.

#### Acquiring a lock
Priority donation will be catalzed by a thread's attempt to acquire a lock via the `lock_acquire ()` function. Upon calling `lock_acquire ()`, we will first check to see if the lock is currently in posession by another thread. If so ...

```c
...

if (lock -> holder != NULL)
{
    struct thread *owner = lock -> holder;
    list_insert_ordered (owner -> donor_list, thread_current (), DESC, NULL);    
}

...
```

At this point, `sema_down ()` will put the current thread on the waitlist for the lock and block the thread until further notice. However, before we completely put the thread to sleep, we need it to contribute a donation to the thread that currently has the lock. We will do this **after** the thread has been put on the lock's waitlist and **before** we call `block_thread ()`. This ensures that the lock will actually include the thread in its waitlist and notify it, and to ensure that the thread will contribute to the lock's owner in order to expedite its own completion. In simple terms, we will be adding the following revisions for `sema_down ()`:

```c
...

list_push_back (&sema -> waiters, &thread_current () -> elem);

if (thread_current() -> awaited_lock != NULL)
{
    donate_priority();
}

thread_block();

...
```

#### Releasing a lock
Upon releasing a lock, `sema_up ()` is called to alert the waiting threads and unblocks one of these threads that are waiting for the lock that was just released. Currently, `sema_up ()` just unblocks the first thread to be waiting. For the purposes of this portion, we will want to revise `sema_up ()` to pick the next thread with the **highest priority** within the semaphore's wait list. The revisions will resemble somewhat of the following:

```c
...

if (!list_empty (&sema -> waiters))
{
    thread_unblock (list_entry (list_max (&sema -> waiters), MAX_PRIORITY, NULL));
}

sema -> value++;

...
```

#### Computing the effective priority
At initialization, the `effective_priority` will be set to the thread's `priority` value. That is because by default, the effective priority is the same as the default priority if no donations were made. From there, we assume there are only two more instances where athread's effective priority will change: (1) a donation occurred or (2) the thread released a lock. In the first case, we calculate the effective priority to be the highest priority value of any of the thread's donors. In the second case, we want to make sure the thread's priority is reset to its default priority. We can do this by simply having

```c
thread -> effective_priority = thread -> priority;
```

On that note, `priority` will not change when using a priority scheduler; it will maintain the default value before donations ever occurred.

#### Priority scheduling for locks and semaphores
The priority scheduling workflow for semaphores and locks is simple and is essentially abstracted away by the semaphore functions that already exist. What will happen is ...

1. A thread will try to acquire a lock
2. If the lock is currently not owned by anyone, set the lock's owner to the current thread and continue running the thread
3. If the lock is owned by another thread, use `sema_down ()` to add the current thread to the lock’s waiters list and have the current thread donate its priority to the lock’s owner
4. Block the current thread
5. Schedule the next thread to run; ideally, it will be the thread that currently owns the lock
6. Upon completion, the lock owner thread release the lock
7. Upon releasing the lock, the lock owner will also reset its priority to its default priority
8. `sema_up ()` will now choose to unblock the highest priority thread; this thread should inherit all of the current thread’s donors
9. The unblocked thread now owns the lock and is scheduled to run as the scheduler sees fit


#### Priority scheduling for condition variables
Since condition variables notify waiters via calling semaphore functions, it will be enough to alter priority scheduling behavior by only revising the semaphore functions as they are explained in the previous subsection. That is, the semaphore functions will abstract away the need to notify and unblock waiting threads. The scheduler will then be responsible for picking the correct semaphore.


#### Changing thread's priority
There are two ways to change the thread's priority. We will maintain the distinction by having two separate functions: (1) `thread_set_priority ()` and (2) `donate_priority ()`.

1. `thread_set_priority` -- We will solely allow the `thread_set_priority ()` function to be used if the current thread wants to update its own priority. For the purposes of the priority scheduler, this will mostly be used when the current thread has released its lock and will now relinquish the priority that was donated to it. Because this priority reset could happen in the middle of a thread's instructions, we will be adding additional resetting features to `thread_set_priority ()`:

    ```c
    void
    thread_set_priority (int new_priority) 
    {
        int max_priority = list_entry (list_max (&ready_list), MAX_PRIORITY, NULL));
        thread_current ()->priority = new_priority;

        if (new_priority < max_priority)
        {
            thread_yield ();
        }   
    }
    ```

2. `donate_priority` -- The other we will be changing the thread's priority wll be through a call to `donate_priority ()`, which will be the only way for the current thread to change another thread's priority. In the simplest form, our function will resemble something as follows:

    ```c
    void
    donate_priority (void)
    {
        if (thread_current() -> awaited_lock == NULL)
        {
            return;
        }

        struct thread *donor = thread_current ();
        struct thread *beneficiary = thread_current () -> awaited_lock -> holder;

        if (beneficiary != NULL)
        {
            if (beneficiary -> effective_priority < donor -> effective_priority)
            {
                beneficiary -> effective_priority = donor -> effective_priority
            }
        }
    }
    ```

### Synchronization

One potential instance of concurrent access in our revisions for the priority scheduler is the fact that we are now allowing one thread the ability to access and change the priority of another thread. One possibility is that we could have two threads that want to donate to the same thread at the same time. This, of course, could create concurrency issues so we would want to resolve this with a primitive such as a lock, for example. We would add a lock field to each thread that allows an external thread the ability to change its priority value. This lock will only be accessible to one thread at a time, thus preventing race conditions. Including a lock field to each thread instead of a global lock for all threads to update priority is an optimization. 

Consider the following scenario: You have thread *A* dependent on thread *C*, and thread *B* dependent on thread *D*. Suppose you run *A*, take the key, and just before donation, you switch to run thread *B*. *B* will no longer have access to the key and on top of that, we just made thread *B* dependent on *A*. If we create a new key for each thread, we sacrifice some space in order to make sure no threads are blocked in the process of priority donation! 

### Rationale

Originally, we planned to have most of the changes happen in `next_thread_to_run ()`, which chooses the next thread that will be executed upon calling `schedule ()`. However, we realized that the only way to detect dependency from the current thread to another thread is if the other thread is holding a lock to access memory that the current thread is trying to access. If we continued the implementation in this way, we would have to know which lock the current thread is trying to acquire. Are there predefined locks that we could access depending on what the current thread is trying to access? How do we find out what the current thread is trying to access? Needless to say, we were unsure as to what lock the current thread wants.

The solution is to abstract away the need to know what the lock is. We achieve this by handling priority donations within `lock_acquire ()` instead: if a thread tries to acquire a lock, it will trigger a potential donation. From there, the rest of the logic just contributes to the overall workflow for donating and semaphore updating.

This solution was much easier to grasp and conceptualize than our previous solution. It takes advantage of the code that is already given to us, thus requiring less lines of code. It also makes good use of the underlying notion that **"if a thread _A_ is dependent on another thread _B_, they must share a lock that _A_ will try to acquire from _B_"** to abstract away lock-specific handling. Additionally, splitting specific tasks across different functions (e.g. `donate_priority ()`) makes it much easier to switch priority scheduling on/off when we also start accommodating MLFQS. Since this solution only introduces three new fields to the thread, it is also minimal in additional space usage.

## Task 3: Multi-level Feedback Queue Scheduler (MLFQS)

### Data structures and functions

The following members will be added to the thread struct:

1. `struct fixed_point_t recent_cpu` -- Keeps track of the amount of time the thread owns the CPU

The following variables will be added to `thread.c`:

1. `static struct fixed_point_t load_avg` -- Keeps track of the weighted average of the number of threads ready to run in the past minute; initialized to 0

We will also be implementing and adding several new functions to `thread.c`:

1. `int thread_get_nice (void)` -- Returns the current thread's niceness
2. `void thread_set_nice (void)` -- Sets the current thread's niceness and calls the `thread_update_priority` function
3. `void thread_update_priority (void)` -- Updates the current thread's priority and yields the thread if it no longer has the highest priority
4. `int thread_get_recent_cpu (void)` -- Returns the `recent_cpu` field scaled by 100 and then rounded
5. `int thread_update_recent_cpu (void)` -- Used as input to the `thread_foreach` function to update the recent `recent_cpu` value for every thread
6. `int thread_get_load_avoid (void)` -- Returns the global variable `load_avg`
7. `list_less_func mlfqs_less_func (const struct list_elem *a, const struct list_elem *b, void *aux)` -- Used as input to `list_insert_ordered` so that a thread will be added to the ready at the end of its given priority levels and just before the next-lowest priority

### Algorithms

#### Updating `recent_cpu` and `load_avg`
Once per second, the `load_avg` variable needs to be recalculated. This can be done within the `timer_interrupt ()` function to avoid race conditions. Within `timer_interrupt ()`, add an `if` statement to check if the current tick falls on a second (by modding with `TIMER_FREQ`) and update `load_avg` with a sequence of fixed point operations. After this update, new values can be calculated for every thread's `recent_cpu` member. We will use the `thread_foreach` function with the `thread_update_recent_cpu` function as the mapping function. Finally, we reupdate the priority of every thread using the `thread_foreach` function with the `thread_update_priority` function as input and resort the ready queue with `list_sort`.

#### Updating current thread's niceness
When the current thread's niceness if updated using the `thread_set_nice` function, its priority should be updated. AFter niceness is updated, call `thread_update_priority`. If the niceness was increased, call `thread_yield` as well.

#### Maintaining multiple queues
Rather than having 64 different lists to keep track of various priority levels, the ready queue will maintain a sorted order of descending priority. When the `schedule` function is called and if MLFQS mode is active, the current thread will not be pushed to the back of the ready queue but rather inserted into the appropriate position using the `mlfqs_less_func` function. This function will be designed to maintain round-robin order among al threads with the highest priority level by inserting the thread at the end relative to other threads of the same priority, but just before the thread with the next-lowest priority.

### Synchronization

At every second, all of the threads’ priorities are updated and the ready queue is re-sorted. This should not happen in the middle of a thread calling the `thread_set_nice` function, since this could lead to a race condition. The solution is to turn interrupts off within the piece of code where the thread’s priority is updated. That way, it would be impossible for the all-threads-update (inside the timer interrupt) to happen in the middle of a niceness update.

### Rationale
A major shortcoming of this approach is the amount of work that is done within the timer interrupt. Every second, there will be O(nlog(n)) work to do when updating the priorities of all threads and then re-sorting the ready queue. But it makes the most sense to complete this timer-oriented task within the timer interrupt. Another shortcoming is the need to turn interrupts off when the current thread updates its niceness value. The more elegant way to solve the problem might be to use a lock, but since interrupts cannot acquire locks, the next best thing is to disable interrupts entirely for this part of the code. The algorithms will all take O(1) space, since the given list sort function.

## Design Document Additional Questions

In order to prove the existence of this bug, we will design a test case that has a semaphore containing two threads in its waitlist. Thread *A* will have an effective priority of 40 and a base priority of 31 while Thread *B* will have an effective priority of 36 and a base priority of 34. If the semaphore is misusing the base priority as its unit of determining the next thread, it will unblock thread *B*; disregarding *A*'s effective priority. Our test will resemble somewhat of the following:

```c
void
test_sema_effective_usage (void)
{
    struct semaphore *sema;
    sema_init (&sema, 1);

    tid_t atid = thread_create ("A", 31, test_sema_effective_helper, &sema);
    thread_set_priority (40);

    tid_t btid = thread_create ("B", 34, test_sema_effective_helper, &sema);
    thread_set_priority (36);

    sema_up ();

    ASSERT (!thread_name().strcmp("A"));
}

static void
acquire_thread_func (struct semaphore *sema) 
{
    sema_down (sema);
}

```

---


    timer ticks | R(A) | R(B) | R(C) | P(A) | P(B) | P(C) | thread to run
    ------------|------|------|------|------|------|------|--------------
     0          | 0    |0     |0     |63    |61    |59    |A
     4          | 4    |0     |0     |62    |61    |59    |A
     8          | 8    |0     |0     |61    |61    |59    |B
    12          | 8    |4     |0     |61    |60    |59    |A
    16          |12    |4     |0     |60    |60    |59    |B
    20          |12    |8     |0     |60    |59    |59    |A
    24          |16    |8     |0     |59    |59    |59    |C
    28          |16    |8     |4     |59    |59    |58    |B
    32          |16    |12    |4     |59    |58    |58    |A
    36          |20    |12    |4     |58    |58    |58    |C

---

There was some ambiguity in deciding which thread to run next in the case of a tie after priority value calculations. We decided that at every priority update, the updated thread should be placed at the back of the queue for its specific priority value.

