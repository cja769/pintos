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
#include <string.h>
#include <stdarg.h> 

/* Protoypes */
void file_index_increment ();
static void syscall_handler (struct intr_frame *);

/* file_index_increment - a function to incremement the file index inside the thread struct
 *   that wraps around the the beginning of the array if file_index is > >= 128. Once the
 *   index wraps around, it searches for an index in the file_list array that is null  
 *   (i.e. that file has been removed)
 */
void file_index_increment () {
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


/* get_arg - a function that gets a desired argument from the stack, checking if it is a valid
 *   argument 
 */
int
get_arg (int *esp, bool is_pointer)
{
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
      // sema_up(&copy->mutex);
      //printf ("Creating zombie process!!!\n");
      break;
    }
  }

  char *saveptr;
  printf ("%s: exit(%d)\n", strtok_r(copy->name, " ", &saveptr), copy->exit_status); // SHOULD WE USE PRINTF HERE?!?

  sema_up(&copy->mutex);

  /* Close and allow write to files inode */
  file_allow_write(t->exe);
  file_close(t->exe); 
  thread_exit();
}

pid_t exec (const char *cmd_line) {
  return process_execute (cmd_line);
}

int wait (pid_t pid) {
  //printf ("pid: %d\n", pid);
  return process_wait (pid);
}

bool create (const char *file, unsigned initial_size) {
  if (file == NULL)
    exit(-1);
  return filesys_create (file, initial_size);
}

bool remove (const char *file) {
  bool result;
  /* Acquire a lock to read */
  lock_acquire(thread_current ()->io_lock);
  result = filesys_remove (file);
  lock_release (thread_current ()->io_lock);
  return result;
}

int open (const char *file)
{
  if (file == NULL) {
    exit(-1);
  }

  struct thread *t = thread_current ();
  struct file *_file = NULL;
  // printf ("In open: %s\n", file);
  _file = filesys_open (file);
  if(_file == NULL)
  {
    // printf ("Null file!\n");
    return -1;
  }
  // Put it in thread array file_list
  file_index_increment();
  t->file_list[t->file_index] = palloc_get_page(0);
  t->file_list[t->file_index] = _file;
  // printf("%d\n", *(t->file_list[t->file_index])->blah);
  // memcpy (t->file_list[t->file_index], file, strlen (file + 1));
  t->file_index++;;
  return t->file_index + 1; // -1 + 2 GO BACK AND FIX THIS
}

int filesize (int fd) {
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
  int result;
  //Checking to see if fd is in the valid range (130 because we are shifting to account for stdin/out)
  if (fd >= 130 || fd < 0 || fd == 1)
      exit(-1);
  struct thread *t = thread_current();
  if(fd >= 2){
    fd -= 2;
    if (t->file_list[fd] == -1)
      exit(-1);
    /* Acquire a lock to read */
    lock_acquire(thread_current ()->io_lock);
    result = file_read (t->file_list[fd], buffer, size);
    lock_release (thread_current ()->io_lock);
    return result;
    }
  else if(fd == 0){
    unsigned i ;
    uint8_t *buffer_ = buffer;
    for(i = 0; i < size; i++){
      char temp = input_getc(); // Storing the return value
      memcpy(&temp, buffer_, sizeof(char));
    }
    /* Acquire a lock to read */
    lock_acquire(thread_current ()->io_lock);
    result = file_read (t->file_list[fd], buffer, size);
    lock_release (thread_current ()->io_lock);
    return result;
  }
  /* Acquire a lock to read */
    lock_acquire(thread_current ()->io_lock);
    result = file_read (t->file_list[fd], buffer, size);
    lock_release (thread_current ()->io_lock);
    return result;
}

int write (int fd, const void *buffer, unsigned size)
{
  int result;
  //Checking to see if fd is in the valid range (130 because we are shifting to account for stdin/out)
  if (fd >= 130 || fd < 0)
      exit(-1);
  //Get file name from fd
  if (fd >= 2) {
    fd -= 2;
    struct thread *t = thread_current();
    if (t->file_list[fd] == -1)
      exit(-1);
    /* Acquire a lock to read */
    lock_acquire(thread_current ()->io_lock);
    result = file_write(t->file_list[fd], buffer, size);
    lock_release (thread_current ()->io_lock);
    return result;
  }

  //Special case for writing to console
  if (fd == 1) {
    putbuf((char *)buffer, size);
  }
  return 0;
}

void seek (int fd, unsigned position) {
  //Checking to see if fd is in the valid range (130 because we are shifting to account for stdin/out)
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
  //Checking to see if fd is in the valid range (130 because we are shifting to account for stdin/out)
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
  //Checking to see if fd is in the valid range (130 because we are shifting to account for stdin/out)
  if (fd >= 130 || fd < 0)
      exit(-1);
  struct thread *t = thread_current();
  // printf("fd: %d\n", t->file_list[fd]);
  if (fd >= 2)
    fd -=2;
  if (t->file_list[fd] == -1)
      exit(-1);
  if (t->file_list[fd] != -1) {
    struct file * file = t->file_list[fd];
    file_close (file);
    t->file_list[fd] = -1;
    //file_allow_write (file); /* Reset the inode */
  }
}

static void
syscall_handler (struct intr_frame *f) 
{
  if (f->esp == NULL || !is_user_vaddr(f->esp) || pagedir_get_page(thread_current()->pagedir, f->esp) == NULL)
    exit(-1);

  int syscall_number = *(int*)f->esp;
  int *myesp = (int*)f->esp;
  myesp++; // Got syscall_number, increment stack pointer

  if (myesp == NULL || !is_user_vaddr(myesp) || pagedir_get_page(thread_current()->pagedir, myesp) == NULL)
    exit(-1);

 // printf("Syscall number: %d\n", syscall_number);
  // int status;

  // printf ("Syscall: %d\n", syscall_number);
  // printf ("system call!\n");

  /*while (myesp)
  {
    printf ("Myesp: %p %d\n", myesp, *myesp);
    myesp ++;
  }*/
  switch (syscall_number) {
    case 0: { //HALT
      halt();
      break;
    }

    case 1: { //EXIT 
      int status = get_arg(myesp, false);
      f->eax = status; // We might not need this... 
      exit(status);
      break;
    }

    case 2: { //EXEC
      char *cmd_line = (char *)get_arg(myesp, true);
      //printf ("Exec-ing...\n");
      f->eax = exec (cmd_line);
      //printf ("Exec-ed\n");
      break;
    }

    case 3: { //WAIT
      pid_t pid = get_arg(myesp, false);
      f->eax = wait(pid);
      break;
    }

    case 4: { //CREATE
      char * file = (char *)get_arg(myesp, true);
      myesp++;
      int initial_size = get_arg(myesp, false);
      f->eax = create(file, initial_size);
      break;
    }

    case 5: { //REMOVE
      char * file = (char *)get_arg(myesp, true);
      f->eax = remove(file);
      break;
    }

    case 6: { // OPEN
      char *arg = (char *)get_arg (myesp, true);
      // printf ("Opening... %s\n", arg);
      f->eax = open (arg);
      // printf ("Opened\n");
      break;
    }

    case 7: { //FILESIZE
      int fd = get_arg(myesp, false);
      f->eax = filesize(fd);
      break;
    }

    case 8: { //READ
      int fd = get_arg(myesp, false);
      myesp++;
      void * buffer = (void *)get_arg(myesp, true);
      myesp++;
      int size = get_arg(myesp, false);
      f->eax = read(fd, buffer, size);
      break;
    }

    case 9: { // WRITE
      int fd = get_arg(myesp, false);
      myesp++;
      void * buffer = (void *)get_arg(myesp, true);
      myesp++;
      int size = get_arg(myesp, false);
      // printf ("Writing... fd = %d, buffer = %s, size = %u\n", fd, (char *)buffer, size);
      f->eax = write(fd, buffer, size);
      // printf ("Wrote...\n");
      break;
    }

    case 10: { //SEEK
      int fd = get_arg(myesp, false);
      myesp++;
      unsigned position = get_arg(myesp, false);
      seek(fd, position);
      break;
    }

    case 11: { //TELL
      int fd = get_arg(myesp, false);
      f->eax = tell(fd);
      break;
    }

    case 12: { //CLOSE
      int fd = get_arg(myesp, false);
      // printf("fd: %d\n", fd);
      close(fd);
      break;
    }
      
    default: 
      // TODO
      printf ("Syscall not yet defined!\n");
      break;
  }
  // thread_exit ();   //Was here originally, but seems wrong
}
