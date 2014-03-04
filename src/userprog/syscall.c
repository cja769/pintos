#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);

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

static void
syscall_handler (struct intr_frame *f) 
{
  int syscall_number = *(int*)f->esp;
  int *myesp = (int*)f->esp;
  myesp++; // Got syscall_number, increment stack pointer

  char *arg;

  printf ("Syscall: %d\n", syscall_number);
  printf ("system call!\n");

  /*while (myesp)
  {
    printf ("Myesp: %p %d\n", myesp, *myesp);
    myesp ++;
  }*/
  switch (syscall_number) {
    case 6: // Open
      arg = get_arg (myesp);
      printf ("Opening... %s\n", arg);
      open (arg);
      break;
    case 9: // Write
      printf ("Writing...\n");
      write(1, (int *)-1073742072, 5);
      printf ("Wrote...\n");
      break;
    default: 
      // TODO
      printf ("Syscall not yet defined!\n");
      break;
  }
  thread_exit ();
}

int open (const char *file)
{
  struct thread *t = thread_current ();
  struct file *_file;
  printf ("In open!\n");
  _file = filesys_open (file);
  if(_file == NULL)
  {
    // Maybe close it?
    printf ("Null file!\n");
    return -1;
  }
  // Put it in thread array file_list
  memcpy (t->file_list[t->file_index], file, strlen (file + 1));
  t->file_index++;
  printf ("Returning fd: %d", t->file_index + 1);
  return t->file_index + 1; // -1 + 2 GO BACK AND FIX THIS
}

int write (int fd, const void *buffer, unsigned size)
{
  // Validate the pointer to buffer
  // TODO
  file_write ();
}
