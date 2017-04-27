# Final Report for Project 1

## Changes to Design Document:

**Preventing Race Conditions Throughout:**

We had to modify certain functions to make them atomic. To this end, we would disable interrupts and re-enable them when we finished working on critical sections. Among other miscellaneous functions, the following functions were made atomic: ``sema_up ()``, ``sema_down ()``, ``lock_release ()``, and ``lock_acquire ()``. The primary reason to make functions atomic was that there were instances in which lists were being altered in the middle of access because context switches were still enabled. This caused kernel panics and forms of NullPointerExceptions when trying to access data that had been deleted.

**Changes to Alarm Clock Sleeping Queue:**

In our design doc, we specified that we would implement a sleeping queue by inserting ``Thread structs`` into a wrapper class, and then building a linked list out of a chain of these wrapper classes. Once we fully understood the linked list implementation that was given with the Pintos source code, we changed this approach by using the default ``list_elem elem`` member of the ``thread Struct`` to insert into a Pintos list. We chose to use elem because we noticed a thread’s sleeping and ready states were mutually exclusive. That is, a thread would never have to be in both the ready list and the sleeping queue, so it would not cause a conflict to use a single list element for both purposes.

**Disabling Interrupts to Interact With The Sleeping Queue:**

In our original design, we discussed using a lock when adding a thread to the sleeping queue. In our final implementation, we chose to simplify this design by simply turning off interrupts when the currently running ``thread`` calls ``timer_sleep()``. The runtime of adding a thread to the ordered sleeping list is **O(n)** where n is the number of currently sleeping threads. This operation is quick enough that it won’t keep interrupts disabled for too long.

**Frequency of Thread Wakeup:**

We were originally going to wake up threads within the timer interrupt handler every 100 ticks, but our design changed after we discussed with Nathan the concept of premature optimization. We decided that checking the condition ``(timer_ticks() % 100 == 0)`` every tick was needlessly expensive, and that a simpler and equally efficient approach would be to scan the sleeping queue every tick. In our final design, at every timer tick, we iterate through the ordered sleeping list until we find a thread that should not be woken up yet. At this point, we stop the iteration since any thread past this one in the list must also be ineligible for waking up at the current tick. So with the final design, we gain extremely precise wakeup times for around the same average overhead as our original design.

**Priority Signaling for Condition Variables:**

While doing priority scheduling for semaphores, we neglected to also do priority-based scheduling for condition variables. In particular, one revision we made since the initial design was to have condition variables signal the highest-priority waiter in ``cond_signal ()``. To that end, we created a new comparator function which compares a conditional variable’s semaphore waiters and signal the semaphore with the highest priority waiter.

**Changes to lock_release ()**

In the design document, we formulated the following workflow for ``lock_release ()``

1. A thread releases a lock
2. The releasing thread resets its priority to the default value
3. Call ``sema_up ()`` to unblock the waiting thread with the highest priority 
4. The unblocked thread now owns the lock, and will execute as the scheduler sees fit

This design would not work for a thread holding multiple locks. Suppose a thread still holds a lock after releasing some other lock. Then the priority of the releasing thread should be the max priority of the threads waiting on the remaining lock. Therefore, it is incorrect to set the priority of the releasing thread back to default. 

We addressed this issue by modifying ``lock_release ()``. ``lock_release`` now looks through the releasing thread's waiter list, and filters all threads that were waiting on the lock that was just released. Then it calls ``sema_up ()``.

Next, we modified ``sema_up ()`` to handle the priority donation after a lock has been released. ``sema_up ()`` now calls ``thread_reset_priority ()``, which computes and sets a priority for the current thread based on its remaining waiters after ``lock_release ()`` filtered out threads that are no longer waiting on a lock owned by the current thread. 

**Changes to thread_create ()**

One error we came across while debugging was that newly created threads would not preempt the current running thread if they had a higher priority. To remedy this, we appended some minor changes into the ``thread_create ()`` function that checks whether the newly created thread has a higher priority than the current thread; if so, we yield the current thread’s resources and have the scheduler pick the highest-priority thread to run.

**Adding Switches For MLFQS Mode:**

A problem that we hadn’t anticipated during the design phase was the interference of priority donation logic in MLFQS mode. In our original design, we did not prevent priority donation from happening when in MLFQS mode, which led to improper thread execution order when synchronization primitives were involved. We fixed this by adding if statements throughout the source code that only allow priority donation logic to happen when not in MLFQS mode, and only allow MLFQS logic to happen when in MLFQS mode.

**Justification For a Single-List Multi Level Feedback Queue:**

We made an effort to recycle the starter code wherever possible in our design. For example, we decided that creating an array of 64 different list structs to implement the multilevel feedback queue would be a wasteful addition to the kernel stack. We instead chose to use and maintain the default ``ready_list`` with the additional constraint that whenever it’s accessed, the threads stored inside remain in decreasing sorted order of priority.

In addition to recycling the skeleton code, the single-list implementation was MUCH simpler to implement. Consider the following procedures for re-sorting the queue every 4 ticks:

64 list queue implementation | 
------------ |
* For each thread,
  * Recalculate / update priority
  * If priority has changed:
    * Remove from prev list
    * Add to new list 
 
Single List Queue Implementation |
------------ |
* For each thread, recalculate priority
* Use built-in in-place mergesort to re-sort the list

The single-list implementation is drastically simpler, and better utilizes the tools given in the Pintos source code.


## Responsibilities

**Coordination**

David was responsible for coordinating team meetings, enforcing deadlines, and delegating roles and tasks to team members. 

**Design**

During the design phase, Rob analyzed Pintos’ interrupt handler to give a presentation to the team about how it works and how it could be used.

**Alarm**

Ninh implemented non-busy waiting alarm clock by making threads sleep in a priority sleeping queue. We introduced one more status ``SLEEPING`` and separated sleeping threads in their own queue to wake up the one when the time reaches its wakeup time. Sleeping a thread should not be in interrupt context and its wakeup should occur with interrupt enabled. The sleeping queue is in the order of the earliest wakeup time, this makes the thread wakeup easier and more efficient by just popping the front thread in the sleeping queue.

**Priority Scheduling**

The priority scheduling tasks, which included implementing priority donation logic, updating and resetting priorities, and notifying synchronization primitives, were delegated to David and Derek. For efficiency, scheduling tasks were further split between David and Derek, with David working on the donation and Derek working on donation list filtering. Paired programming was exercised in the beginning when developing lock acquiring and releasing to gain a better conceptual understanding of the subject and to iron out the work that needed to be done at each step.

**MLFQS**

Rob designed and implemented the full MLFQS part of the project. Included in his duties were:
* Deciding order of operations when calculating `load_avg` and `recent_cpu` values
* Deciding to make a ``load_avg`` *static global variable*.
* Comparing and contrasting the 64-List and Single-List queue implementations mentioned above
* Identifying race conditions in setting thread niceness
* Choosing the order that the MLFQS related variables are calculated in every so many ticks (All threads’ priority -> load_avg -> recent_cpu)

**Debugging**

All team members participated in debugging the final result, with Rob and David being the primary debuggers for the final integration tests (i.e. tests that interacted with two or more tasks). Derek spearheaded the active use of GDB to find problems in the code, and instructed the rest of the group on how to use the tool.

**Improvements**

* Workload for coding and debugging imbalanced (require more communication)
* More frequent meetings, that don’t span three consecutive days
  * Friday afternoons and Wednesday meetup?
* Code presentations were good for conceptual understanding
* Group members need to be more prepared for meetings
* Projectors were useful during presentations
* Consider having meetings on Saturdays @Soda
* Meeting up over the Internet, as supplement to existing meetings
* Create a Google Doc of questions for OH
* Engage with the skeleton code early on in the design process (before the design doc is due)

**Other Comments**

* There was ambiguity regarding what needed to be done.
(David) Need to give out more well-defined tasks at the end of each meeting
* Meetings should not be meant for solo-programming; use time to ask conceptual questions, review code, and discuss next steps
* Start reading the project specs earlier (before first meeting)




