#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "lib/kernel/console.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "userprog/pagedir.h"
#include "devices/block.h"
#include <string.h>
#include <stdarg.h> 

/* Protoypes */
void file_index_increment ();
static void syscall_handler (struct intr_frame *);

/* file_index_increment - a function to increment the file index inside the thread struct
 *   that wraps around the the beginning of the array if file_index is > >= 128. Once the
 *   index wraps around, it searches for an index in the file_list array that is null  
 *   (i.e. that file has been removed)
 */
void file_index_increment () {
  // Calvin drove this method 
  struct thread *t = thread_current();
  int index = t->file_index;
  index--;
  do{
    index++;
    if (index >= 128) {
      index = 0;
      t->wrap_flag =1;
    } 
  } while( t->wrap_flag && t->file_list[index] != NULL);
  t->file_index = index;
}

bool parse_path(const char *name) {
  // char **saveptr = NULL; //used for strtok_r
  // char *temp = NULL;
  // struct inode *inode = NULL;

  // temp = strtok_r(name, "/", saveptr);
  // if(*name == '/') {
  //   if(dir_lookup(dir_open_root(), temp, &inode) == NULL)
  //     return false;
  //   t->directory = dir_open(inode);
  // }

  // while(temp != NULL) {
  //   if(dir_lookup(t->directory, temp, &inode) == NULL)
  //     return false;
  //   t->directory = dir_open(inode); 
  //   temp = strtok_r(NULL, "/", saveptr));
  // }
  // return true;
}

/* get_arg - a function that dereferences an argument from the stack, checking if it is a valid */ 
int
get_arg (int *esp, bool is_pointer)
{
  // Calvin drove this method
  if (is_pointer) {
    if ((int *)*esp == NULL || !is_user_vaddr((int *)*esp) || pagedir_get_page(thread_current()->pagedir, (int *)*esp) == NULL)
      exit(-1);
  }
  return *esp;
}

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* BEGINNING SYSTEM CALLS */

void halt (void) {
  shutdown_power_off();
}

void exit (int status) {
  // Calvin and Samantha took turns driving this system_call
  /* Get the copy of this thread from the parent, and set its exit status */
  struct thread *t = thread_current ();

  /* Loop through the parents list of children to find the copy that matches this thread... */
  struct list_elem *e;
  struct thread *copy;

  for (e = list_begin (&t->parent->children); e != list_end (&t->parent->children);
       e = list_next (e))
  {
    copy = list_entry (e, struct thread, elem);
    if (copy->tid == t->tid)
    {
      // Create the zombie process for the parent
      copy->exit_status = status; 
      copy->status = THREAD_DYING;
      break;
    }
  }

  char *saveptr;
  printf ("%s: exit(%d)\n", strtok_r(copy->name, " ", &saveptr), copy->exit_status); 

  // When a process exits, sema_up to allow its parent to return
  sema_up(&copy->mutex); 

  /* Close and allow write to files inode for rox */
  file_allow_write(t->exe);
  file_close(t->exe); 
  thread_exit();
}

pid_t exec (const char *cmd_line) {
  // Jason drove this method
  /* Returns the tid of a new child thread */
  return process_execute (cmd_line);
}

int wait (pid_t pid) {
  // Jason drove this system call
  /* Blocks the parents thread and returns the exit status of the child that terminated */
  return process_wait (pid);
}

bool create (const char *file, unsigned initial_size) {
  // Samantha drove this method
  /* Create a new file with name file of size initial_size and return true if successful */
  if (file == NULL)
    exit(-1);
  return filesys_create (file, initial_size);
}

bool remove (const char *file) {
  // Samantha drove this method
  /* Remove a file from the file_sys and return true if successful */
  /* Acquire a lock to remove from the file_sys */
  lock_acquire(thread_current ()->io_lock);
  bool result;
  result = filesys_remove (file);
  lock_release (thread_current ()->io_lock);
  return result;
}

int open (const char *file)
{
  // Calvin and Samantha took turns driving this system_call
  /* Opens a file in file_sys and returns a file_descriptor for this threads list of open files */
  if (file == NULL) {
    exit(-1);
  }

  struct thread *t = thread_current ();
  struct file *_file = NULL;
  _file = filesys_open (file);
  if(_file == NULL)
  {
    return -1;
  }
  // Put it in thread array file_list
  file_index_increment();
  t->file_list[t->file_index] = palloc_get_page(0);
  t->file_list[t->file_index] = _file;
  t->file_index++;;
  return t->file_index + 1; 
}

int filesize (int fd) {
  // Samantha drove this method
  // Returns the size of a file in file_sys
  //Checking to see if fd is in the valid range (130 because we are shifting to account for stdin/out)
  if (fd >= 130 || fd < 0)
      exit(-1);
  struct thread *t = thread_current();
  if (fd >= 2)
    fd -= 2;
  struct file *file = t->file_list[fd];
  return file_length (file);
}

int read (int fd, void *buffer, unsigned size) {
  // Calvin and Samantha took turns driving this system_call
  struct thread *t = thread_current();
  int result;
  //Checking to see if fd is in the valid range (130 because we are shifting to account for stdin/out)
  if (fd >= 130 || fd < 0 || fd == 1)
  {
      exit(-1);
  }
  /* If the file is in file_sys, read and release the lock */
  if(fd >= 2){
    fd -= 2;
    if (t->file_list[fd] == -1)
  {
      exit(-1);
  }
    /* Acquire a lock to read a file in file_sys */
    lock_acquire(t->io_lock);
    result = file_read (t->file_list[fd], buffer, size);
    lock_release (t->io_lock);
    return result;
  }
  else if(fd == 0){
    unsigned i ;
    uint8_t *buffer_ = buffer;
    for(i = 0; i < size; i++){
      char temp = input_getc(); // Storing the return value
      memcpy(&temp, buffer_, sizeof(char));
    }
    /* Acquire a lock to read a file in file_sys */
    lock_acquire(t->io_lock);
    result = file_read (t->file_list[fd], buffer, size);
    lock_release (t->io_lock);
    return result;
  }
    /* Acquire a lock to read a file in file_sys */
    lock_acquire(t->io_lock);
    result = file_read (t->file_list[fd], buffer, size);
    lock_release (t->io_lock);
    return result;
}

int write (int fd, const void *buffer, unsigned size)
{
  // Calvin and Samantha took turns driving this system_call
  struct thread *t = thread_current();
  int result;
  // Checking to see if fd is in the valid range (130 because we are shifting to account for stdin/out)
  if (fd >= 130 || fd < 0)
  {
      exit(-1);
  }
  // Get file name from fd
  if (fd >= 2) {
    fd -= 2;
    if (t->file_list[fd] == -1)
  {
      exit(-1);
  }
    /* Acquire a lock to write to file_sys */
    lock_acquire(t->io_lock);
    result = file_write(t->file_list[fd], buffer, size);
    lock_release (t->io_lock);
    return result;
  }

  // Special case for writing to console
  if (fd == 1) {
    putbuf((char *)buffer, size);
  }
  return 0;
}

void seek (int fd, unsigned position) {
  // Samantha drove this method
  // Checking to see if fd is in the valid range (130 because we are shifting to account for stdin/out)
  if (fd >= 130 || fd < 0)
      exit(-1);
  struct thread *t = thread_current();
  if (fd >= 2)
    fd -=2;
  if (t->file_list[fd] == -1)
      exit(-1);
  struct file *file = t->file_list[fd];
  file_seek (file, position);
}

unsigned tell (int fd) {
  // Samantha drove this method
  // Checking to see if fd is in the valid range (130 because we are shifting to account for stdin/out)
  if (fd >= 130 || fd < 0)
      exit(-1);
  struct thread *t = thread_current();
  if (fd >= 2)
    fd -=2;
  if (t->file_list[fd] == -1)
      exit(-1);
  struct file * file = t->file_list[fd];
  return file_tell (file); 
}

void close (int fd) {
  // Calvin and Samantha took turns driving this system_call
  // Checking to see if fd is in the valid range (130 because we are shifting to account for stdin/out)
  if (fd >= 130 || fd < 0)
      exit(-1);
  struct thread *t = thread_current();
  if (fd >= 2)
    fd -=2;
  if (t->file_list[fd] == -1)
      exit(-1);
  if (t->file_list[fd] != -1) {
    struct file * file = t->file_list[fd];
    file_close (file);
    t->file_list[fd] = -1;
  }
}

bool chdir(const char *dir) {
  // struct thread *t = thread_current();
  // struct inode *inode = NULL;
  // char *temp = NULL;
  // char **saveptr = NULL; //used for strtok_r

  // temp = strtok_r(dir, "/", saveptr);
  // if(*dir == '/') {
  //   // dir_lookup()
  // }
  // dir_lookup (t->directory, temp, &inode);
  // while((temp = strtok_r(NULL, "/", saveptr)) != NULL) {
  //   dir_lookup (t->directory, temp, &inode);
  // }
}

bool mkdir(const char *dir) {
  // if(!parse_path(dir)) {
  //   block_sector_t *sector_idx = calloc(1, sizeof int);
  //   free_map_allocate(1, sector_idx);
  //   dir_create(sector_idx, 0); //Size might not be zero
  // }
}

bool readdir(int fd, char *name) {

}

bool isdir(int fd) {

}

int inumber(int fd) {

}

static void
syscall_handler (struct intr_frame *f) 
{
  // Dustin, Jason, Samantha, and Calvin took turns driving this method
  if (f->esp == NULL || !is_user_vaddr(f->esp) || pagedir_get_page(thread_current()->pagedir, f->esp) == NULL)
    exit(-1);

  int syscall_number = *(int*)f->esp;
  int *myesp = (int*)f->esp;
  myesp++; // Got syscall_number, increment stack pointer

  if (myesp == NULL || !is_user_vaddr(myesp) || pagedir_get_page(thread_current()->pagedir, myesp) == NULL)
    exit(-1);

  /* This is OFTEN used for debugging and tracing the system calls pintos makes */
  // printf("Syscall number: %d\n", syscall_number);

  switch (syscall_number) {
    case 0: { //HALT
      halt();
      break;
    }

    case 1: { //EXIT 
      int status = get_arg(myesp, false);
      f->eax = status; // Update register with return value
      exit(status);
      break;
    }

    case 2: { //EXEC
      char *cmd_line = (char *)get_arg(myesp, true);
      f->eax = exec (cmd_line); // Update register with return value
      break;
    }

    case 3: { //WAIT
      pid_t pid = get_arg(myesp, false);
      f->eax = wait(pid); // Update register with return value
      break;
    }

    case 4: { //CREATE
      char * file = (char *)get_arg(myesp, true);
      myesp++;
      int initial_size = get_arg(myesp, false);
      f->eax = create(file, initial_size); // Update register with return value
      break;
    }

    case 5: { //REMOVE
      char * file = (char *)get_arg(myesp, true);
      f->eax = remove(file); // Update register with return value
      break;
    }

    case 6: { // OPEN
      char *arg = (char *)get_arg (myesp, true);
      f->eax = open (arg); // Update register with return value
      break;
    }

    case 7: { //FILESIZE
      int fd = get_arg(myesp, false);
      f->eax = filesize(fd); // Update register with return value
      break;
    }

    case 8: { //READ
      int fd = get_arg(myesp, false);
      myesp++;
      void * buffer = (void *)get_arg(myesp, true);
      myesp++;
      int size = get_arg(myesp, false);
      f->eax = read(fd, buffer, size); // Update register with return value
      break;
    }

    case 9: { // WRITE
      int fd = get_arg(myesp, false);
      myesp++;
      void * buffer = (void *)get_arg(myesp, true);
      myesp++;
      int size = get_arg(myesp, false);
      f->eax = write(fd, buffer, size); // Update register with return value
      break;
    }

    case 10: { //SEEK
      int fd = get_arg(myesp, false);
      myesp++;
      unsigned position = get_arg(myesp, false);
      seek(fd, position); // Seek to a new position in the file in file_sys
      break;
    }

    case 11: { //TELL
      int fd = get_arg(myesp, false);
      f->eax = tell(fd); // Update register with return value
      break;
    }

    case 12: { //CLOSE
      int fd = get_arg(myesp, false);
      close(fd); // Close a file in file_sys
      break;
    }

    case SYS_CHDIR: { 
      char *dir = (char *)get_arg(myesp, true);
      f->eax = chdir(dir);
      break;
    }

    case SYS_MKDIR: {
      char *dir = (char *)get_arg(myesp, true);
      f->eax = mkdir(dir);
      break;
    }

    case SYS_READDIR: {
      int fd = get_arg(myesp, false);
      myesp++;
      char *dir = (char *)get_arg(myesp, true);
      f->eax = readdir(fd, dir);
      break;
    }
      
    case SYS_ISDIR: {
      int fd = get_arg(myesp, false);
      f->eax = isdir(fd);
      break;
    }

    case SYS_INUMBER: {
      int fd = get_arg(myesp, false);
      f->eax = inumber(fd);
      break;
    }
      
    default: 
      printf ("Syscall not yet defined!\n"); // 
    f->eax = -1; // Update register with return value
    exit (-1); // If a system call is not defined, safely exit the thread
      break;
  }
}
