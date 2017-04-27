#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "devices/input.h"
#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"

/* Lock for all file syscalls. */
static struct lock file_sc_lock;
static void syscall_handler (struct intr_frame *);

/* Initializes the syscall handler and lock. */
void
syscall_init (void)
{
  lock_init (&file_sc_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* Maps interrupt frames to the approrpriate syscall. */
static void
syscall_handler (struct intr_frame *f)
{
  uint32_t* args = ((uint32_t *) f->esp);

  if (!has_valid_args (args))
    kill_process (-1);

  switch (args[0])
    {
    case SYS_HALT:
      halt_syscall ();
      return;
    case SYS_EXIT:
      exit_syscall (args[1], (int *) &f->eax);
      return;
    case SYS_EXEC:
      exec_syscall ((char *) args[1], (pid_t *) &f->eax);
      return;
    case SYS_WAIT:
      wait_syscall (args[1], (int *) &f->eax);
      return;
    case SYS_CREATE:
      create_syscall ((char *) args[1], args[2], (int *) &f->eax);
      return;
    case SYS_REMOVE:
      remove_syscall ((char *) args[1], (int *) &f->eax);
      return;
    case SYS_OPEN:
      open_syscall ((char *) args[1], (int *) &f->eax);
      return;
    case SYS_FILESIZE:
      filesize_syscall (args[1], (int *) &f->eax);
      return;
    case SYS_READ:
      read_syscall (args[1], (void *) args[2], args[3], (int *) &f->eax);
      return;
    case SYS_WRITE:
      write_syscall (args[1], (void *) args[2], args[3], (int *) &f->eax);
      return;
    case SYS_SEEK:
      seek_syscall (args[1], args[2]);
      return;
    case SYS_TELL:
      tell_syscall (args[1], (unsigned *) &f->eax);
      return;
    case SYS_CLOSE:
      close_syscall (args[1]);
      return;
    case SYS_PRACTICE:
      practice_syscall (args[1], (int *) &f->eax);
      return;
    default:
      printf("ERROR: Invalid syscall number inputted\n");
    }
}

/* Terminates the Pintos OS. */
void
halt_syscall (void)
{
  shutdown_power_off ();
}

/* Terminates the current user program and returns the 
   status to the kernel. */
void
exit_syscall (int exit_status, int *output)
{
  *output = exit_status;
  kill_process (exit_status);
}

/* Runs the executable whose name is given in cmd_line,
   passing any given arguments, and returns the new
   process’s program id (pid). */
void
exec_syscall (char* input, pid_t *output)
{
  if (!is_valid_string (input))
    kill_process (-1);

  *output = process_execute (input);
}

/* Waits for a child process pid and retrieves the child’s
   exit status. */
void
wait_syscall (pid_t p, int *output)
{
  *output = process_wait (p);
}

/* Creates a new file called file initially initial size
   bytes in size. */
void
create_syscall (const char *file, unsigned initial_size, int *output)
{
  if (!is_valid_string(file) || strcmp (file, "") == 0)
    kill_process (-1);
  else if (strlen (file) > 14)
    *output = 0;
  else
    {
      lock_acquire (&file_sc_lock);
      *output = filesys_create (file, initial_size);
      lock_release (&file_sc_lock);  
    }
}

/* Deletes the file called file. Returns true if successful;
   false otherwise. */
void
remove_syscall (const char *file, int *output)
{
  if (!is_valid_string (file))
    kill_process (-1);

  lock_acquire (&file_sc_lock);
  *output = filesys_remove (file);
  lock_release (&file_sc_lock);
}

/* Opens the file called file. Returns a nonnegative
   integer handle called a “file descriptor” (fd), or -1
   if the file could not be opened. */
void
open_syscall (const char *file, int *output)
{
  if (!is_valid_string (file))
    kill_process (-1);

  struct file *file_ptr = file_open_sync (file);

  if (file_ptr == NULL)
    *output = -1;
  else
    {  
      struct file_blob *blob = malloc (sizeof (struct file_blob));
      blob->fd = thread_current ()->next_fd++;
      blob->file_ptr = file_ptr;

      list_push_front (&thread_current ()->file_list, &blob->file_elem);

      *output = blob->fd;
    }
}

/* Returns the size, in bytes, of the file open as fd. */ 
void
filesize_syscall (int fd, int *output)
{
  struct file_blob *blob = find_blob_from_fd (fd);

  if (blob == NULL)
    *output = -1;
  else
    {
      lock_acquire (&file_sc_lock);
      *output = file_length (blob->file_ptr);
      lock_release (&file_sc_lock);  
    }
}

/* Reads size bytes from the file open as fd into buffer.
   Returns the number of bytes actually read or -1 if the
   file could not be read. */
void
read_syscall (int fd, void *buffer, unsigned size, int *output)
{
  if (!is_valid_buffer (buffer, size))
    kill_process (-1);
  
  /* Read from the keyboard */
  if (fd == 0)
    {
      unsigned index;
      uint8_t* stdin_buffer = (uint8_t *) buffer;

      for (index = 0; index < size; index++)
        stdin_buffer[index] = input_getc ();

      *output = size;
    }
  else
    {
      struct file_blob *blob = find_blob_from_fd (fd);
      if (blob == NULL)
        *output = -1;
      else 
        {
          lock_acquire (&file_sc_lock);
          *output = file_read (blob->file_ptr, buffer, size);
          lock_release (&file_sc_lock);  
        }
    }
}

/* Writes size bytes from buffer to the open file fd. Returns
   the number of bytes actually written. */
void
write_syscall (int fd, const void *buffer, unsigned size, int *output)
{
  if (!is_valid_buffer (buffer, size))
    kill_process (-1);

  /* Write to standard output: */
  if (fd == 1)
    {
      unsigned index;
      for (index = 0; index < size; index += 256)
        {
          int remaining = size - index;
          int amount = remaining < 256 ? remaining : 256;
          putbuf (buffer + index, amount);
        }
    }
  else
    {
      struct file_blob *blob = find_blob_from_fd (fd);
      if (blob == NULL)
        *output = -1;
      else
        {
          lock_acquire (&file_sc_lock);
          *output = file_write (blob->file_ptr, buffer, size);
          lock_release (&file_sc_lock);    
        }
    }
}

/* Changes the next byte to be read or written in open file
   fd to position, expressed in bytes from the beginning of
   the file. */
void
seek_syscall (int fd, unsigned position)
{
  struct file_blob *blob = find_blob_from_fd (fd);
  if (blob == NULL) return;

  lock_acquire (&file_sc_lock);
  file_seek (blob->file_ptr, position);
  lock_release (&file_sc_lock);
}

/* Returns the position of the next byte to be read or written
   in open file fd, expressed in bytes from the beginning of the
   file. */
void
tell_syscall (int fd, unsigned *output)
{
  struct file_blob *blob = find_blob_from_fd (fd);
  if (blob == NULL)
    *output = -1;
  else
    {
      lock_acquire (&file_sc_lock);
      *output = file_tell (blob->file_ptr);
      lock_release (&file_sc_lock);
    }
}

/* Closes file descriptor fd. Exiting or terminating a process
   implicitly closes all its open file descriptors. */
void
close_syscall (int fd)
{
  struct file_blob *blob = find_blob_from_fd (fd);
  if (blob == NULL) return;

  list_remove (&blob->file_elem);
  file_close_sync (blob->file_ptr);
  free (blob);
}

/* Helper function for opening files in a synchronized fashion: */
struct file *
file_open_sync (const char *file)
{
  lock_acquire (&file_sc_lock);
  struct file *file_ptr = filesys_open (file);
  lock_release (&file_sc_lock);
  return file_ptr;
}

/* Helper function for closing files in a synchronized fashion: */
void
file_close_sync (struct file *f)
{
  lock_acquire (&file_sc_lock);
  file_close (f);
  lock_release (&file_sc_lock);
}

/* Helper function for allowing writes in a synchronized fashion: */
void file_allow_write_sync (struct file *f)
{
  lock_acquire (&file_sc_lock);
  file_allow_write (f);
  lock_release (&file_sc_lock);
}

/* Helper function for denying writes in a synchronized fashion: */
void file_deny_write_sync (struct file *f)
{
  lock_acquire (&file_sc_lock);
  file_deny_write (f);
  lock_release (&file_sc_lock);
}

/* Increments the passed in integer argument by 1 and
   returns it to the user. */
void
practice_syscall (int input, int* output)
{
  *output = input + 1;
}

/* Checks that the args passed into the syscall are valid. */
bool
has_valid_args (const void *args)
{
  int sys_num;
  unsigned num_args;
  uint32_t* args_ptr = (uint32_t *) args;

  if (!is_valid_pointer ((const void *) args_ptr))
    return false;

  sys_num = args_ptr[0];

  if (sys_num == SYS_HALT)
    num_args = 0;
  else if (sys_num == SYS_EXIT || sys_num == SYS_EXEC
           || sys_num == SYS_REMOVE || sys_num == SYS_OPEN
           || sys_num == SYS_FILESIZE || sys_num == SYS_WAIT
           || sys_num == SYS_TELL || sys_num == SYS_CLOSE
           || sys_num == SYS_PRACTICE)
    num_args = 1;
  else if (sys_num == SYS_CREATE || sys_num == SYS_READ
           || sys_num == SYS_WRITE || sys_num == SYS_SEEK)
    num_args = 2;
  else
    return false;

  if (!is_valid_pointer ((const void *) args_ptr + (4 * num_args)))
    return false;

  return true;
}

/* Checks that the given pointer is not a NULL pointer, does not
   read from kernel data, and is not trying to access unmapped
   memory. */
bool
is_valid_pointer (const void *vaddr)
{
  return (is_user_vaddr (vaddr))
          && (pagedir_get_page(thread_current ()->pagedir, vaddr) != NULL);
}

/* Checks that the given string is valid. */
bool
is_valid_string (const char *string)
{
  char* str_ptr = (char *) string;

  while (1)
    {
      if (string == NULL)
        return false;
      else if (!is_valid_pointer ((const void *) str_ptr))
        return false;
      else if (*str_ptr == '\0')
        break;

      str_ptr++;
    }

  return true;
}

/* Checks that the given buffer is valid. */
bool
is_valid_buffer (const void *buffer, unsigned size)
{
  unsigned i;
  char* buff_ptr = (char *) buffer;

  if (buff_ptr == NULL)
    return false;

  for (i = 0; i < size; i++)
    {
      if (!is_valid_pointer ((const void *) buff_ptr))
        return false;
      buff_ptr++;
    }

  return true;
}

/* Exits the current process and sets its exit status. */
void
kill_process (int status)
{
  thread_current ()->kin_struct->exit_status = status;
  thread_exit ();
}

/* Returns the file blob associated to the given file
   descriptor. */
struct file_blob*
find_blob_from_fd (int fd)
{
  struct thread *t = thread_current ();
  struct list *file_list = &t->file_list;
  
  struct list_elem *e;
  struct file_blob *blob = NULL;
  for (e = list_begin (file_list); e != list_end (file_list);
    e = list_next (e))
    {
      blob = list_entry (e, struct file_blob, file_elem);
      if (blob->fd == fd)
        return blob;
    }

  return NULL;
}
