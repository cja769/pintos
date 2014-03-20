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
get_arg (int *esp)
{
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
  /* Find out where to save the exit status... I think Moob told us to 
     make a struct that has the child element in it for each semaphore? */
  
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
        //printf ("Creating zombie process!!!\n");
      }
      continue; // 'break' to thread_exit ()
    }

  printf ("%s: exit(%d)\n", copy->name, copy->exit_status); // SHOULD WE USE PRINTF HERE?!?

  thread_exit();
}

pid_t exec (const char *cmd_line) {
  int tid = process_execute (cmd_line);
  //printf ("Result from process_execute: %d\n", tid);
  return tid;
}

int wait (pid_t pid) {
  //printf ("pid: %d\n", pid);
  return process_wait (pid);
}

bool create (const char *file, unsigned initial_size) {
  printf("%s, %d\n", file, initial_size);
  return filesys_create (file, initial_size);
}

bool remove (const char *file) {
  return filesys_remove (file);
}

int open (const char *file)
{
  struct thread *t = thread_current ();
  struct file *_file = NULL;
  // printf ("In open: %s\n", file);
  _file = filesys_open (file);
  if(_file == NULL)
  {
    // printf ("Null file!\n");
    return -1;
  }
  else
    // printf("Not null!\n");
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
  struct thread *t = thread_current();
  if (fd >= 2)
    fd -= 2;
  struct file *file = t->file_list[fd];
  return file_length (file);
}

int read (int fd, void *buffer, unsigned size) {
  struct thread *t = thread_current();
  if(fd >= 2){
    fd -= 2;
    struct file *file = t->file_list[fd];
    return file_read (file, buffer, size);
    }
  else if(fd == 0){
    unsigned i ;
    uint8_t *buffer_ = buffer;
    for(i = 0; i < size; i++){
      char temp = input_getc(); // Storing the return value
      memcpy(&temp, buffer_, sizeof(char));
    }
    return size;
  }

  return 0; // Control never reaches here
}

int write (int fd, const void *buffer, unsigned size)
{
  //Get file name from fd
  if (fd >= 2) {
    fd -= 2;
    struct thread *t = thread_current();

    return file_write(t->file_list[fd], buffer, size);
  }

  //Special case for writing to console
  if (fd == 1) {
    putbuf((char *)buffer, size);
  }
  return 0;
}

void seek (int fd, unsigned position) {
  struct thread *t = thread_current();
  if (fd >= 2)
    fd -=2;
  struct file *file = t->file_list[fd];
  file_seek (file, position);
}

unsigned tell (int fd) {
  struct thread *t = thread_current();
  if (fd >= 2)
    fd -=2;
  struct file * file = t->file_list[fd];
  return file_tell (file); 
}

void close (int fd) {
  struct thread *t = thread_current();
  if (fd >= 2)
    fd -=2;
  if (t->file_list[fd] != NULL) {
    struct file * file = t->file_list[fd];
    file_close (file);
    t->file_list[fd] = NULL;
  }
}



static void
syscall_handler (struct intr_frame *f) 
{
  int syscall_number = *(int*)f->esp;
  int *myesp = (int*)f->esp;
  myesp++; // Got syscall_number, increment stack pointer

  //printf("Syscall number: %d\n", syscall_number);
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
      int status = get_arg(myesp);
      f->eax = status; // We might not need this... 
      exit(status);
      break;
    }

    case 2: { //EXEC
      char *cmd_line = (char *)get_arg(myesp);
      //printf ("Exec-ing...\n");
      f->eax = exec (cmd_line);
      //printf ("Exec-ed\n");
      break;
    }

    case 3: { //WAIT
      pid_t pid = get_arg(myesp);
      f->eax = wait(pid);
      break;
    }

    case 4: { //CREATE
      char * file = (char *)get_arg(myesp);
      myesp++;
      int initial_size = get_arg(myesp);
      f->eax = create(file, initial_size);
      break;
    }

    case 5: { //REMOVE
      char * file = (char *)get_arg(myesp);
      f->eax = remove(file);
      break;
    }

    case 6: { // OPEN
      char *arg = (char *)get_arg (myesp);
      // printf ("Opening... %s\n", arg);
      f->eax = open (arg);
      // printf ("Opened\n");
      break;
    }

    case 7: { //FILESIZE
      int fd = get_arg(myesp);
      f->eax = filesize(fd);
      break;
    }

    case 8: { //READ
      int fd = get_arg(myesp);
      myesp++;
      void * buffer = (void *)get_arg(myesp);
      myesp++;
      int size = get_arg(myesp);
      f->eax = read(fd, buffer, size);
      break;
    }

    case 9: { // WRITE
      int fd = get_arg(myesp);
      myesp++;
      void * buffer = (void *)get_arg(myesp);
      myesp++;
      int size = get_arg(myesp);
      // printf ("Writing... fd = %d, buffer = %s, size = %u\n", fd, (char *)buffer, size);
      f->eax = write(fd, buffer, size);
      // printf ("Wrote...\n");
      break;
    }

    case 10: { //SEEK
      int fd = get_arg(myesp);
      myesp++;
      unsigned position = get_arg(myesp);
      seek(fd, position);
      break;
    }

    case 11: { //TELL
      int fd = get_arg(myesp);
      f->eax = tell(fd);
      break;
    }

    case 12: { //CLOSE
      int fd = get_arg(myesp);
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
