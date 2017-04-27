#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "userprog/process.h"
#include "threads/interrupt.h"
#include "devices/shutdown.h"

void syscall_init (void);

bool has_valid_args (const void* args);
bool is_valid_pointer (const void *vaddr);
bool is_valid_string (const char *string);
bool is_valid_buffer (const void *buffer, unsigned size);

void halt_syscall (void);
void exit_syscall (int exit_status, int* output);
void exec_syscall (char* input, pid_t *output);
void wait_syscall (pid_t p, int *output);
void create_syscall (const char *file, unsigned initial_size, int *output);
void remove_syscall (const char *file, int *output);
void open_syscall (const char *file, int *output);
void filesize_syscall (int fd, int *output);
void read_syscall (int fd, void *buffer, unsigned size, int *output);
void write_syscall (int fd, const void *buffer, unsigned size, int *output);
void seek_syscall (int fd, unsigned position);
void tell_syscall (int fd, unsigned *output);
void close_syscall (int fd);
void practice_syscall (int input, int* output);
void kill_process (int status);
struct file_blob *find_blob_from_fd (int fd);
struct file *file_open_sync (const char *file);
void file_close_sync (struct file *f);
void file_allow_write_sync (struct file *f);
void file_deny_write_sync (struct file *f);


#endif /* userprog/syscall.h */
