#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "threads/malloc.h"
#include "filesys/filesys.h"
#include "filesys/file.h"

typedef int pid_t;

#include "userprog/syscall.h"

tid_t process_execute (const char *file_name);
int process_wait (pid_t);
void process_exit (void);
void process_activate (void);

char* get_first_tok (char *);
int get_num_args (char *);
void populate_stack_and_argv (char *command, void **sp, char **argv);
void align_stack (void **sp);
void copy_args_and_ra (char **argv, int argc, void **sp);

/* The struct that is shared between a thread and its parent on call to exec. */
struct thread_kinship
  {
    char *cmd_line_input;
    struct semaphore kin_sema;
    struct lock kin_lock;
    pid_t pid;
    int exit_status;
    struct thread *parent_thread;
    struct thread *child_thread;
    struct list_elem child_elem;
  };

/* The struct that represents a process's open file. */
struct file_blob
  {
  	int fd;
  	struct file *file_ptr;
  	struct list_elem file_elem;
  };

#endif /* userprog/process.h */
