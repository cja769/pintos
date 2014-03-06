#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "lib/kernel/console.h"
#include "filesys/filesys.h"
#include "devices/shutdown.h"
#include <string.h>


static void syscall_handler (struct intr_frame *);

/* file_index_increment - a function to incremement the file index inside the thread struct
 *   that wraps around the the beginning of the array if file_index is > >= 128. Once the
 *   index wraps around, it searches for an index in the file_list array that is null  
 *   (i.e. that file has been removed)
 */
void file_index_increment() {
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

static void
syscall_handler (struct intr_frame *f) 
{
  int syscall_number = *(int*)f->esp;
  int *myesp = (int*)f->esp;
  myesp++; // Got syscall_number, increment stack pointer

  char *arg;
  int fd;
  void *buffer;
  int size;
  int status;

  // printf ("Syscall: %d\n", syscall_number);
  // printf ("system call!\n");

  /*while (myesp)
  {
    printf ("Myesp: %p %d\n", myesp, *myesp);
    myesp ++;
  }*/
  switch (syscall_number) {
    case 0: //HALT
      shutdown_power_off();
      break;

    case 1: //EXIT
      status = get_arg(myesp);
      myesp++;
      f->eax = status;
      thread_exit();
      break;

    case 2: //EXEC

      break;

    case 3: //WAIT

      break;

    case 4: //CREATE

      break;

    case 5: //REMOVE

      break;

    case 6: // OPEN
      arg = (char *)get_arg (myesp);
      myesp++;
      printf ("Opening... %s\n", arg);
      open (arg);
      printf ("Opened\n");
      break;

    case 7: //FILESIZE

      break;

    case 8: //READ

      break;

    case 9: // WRITE
      fd = get_arg(myesp);
      myesp++;
      buffer = (void *)get_arg(myesp);
      myesp++;
      size = get_arg(myesp);
      myesp++;
      // printf ("Writing... fd = %d, buffer = %s, size = %u\n", fd, (char *)buffer, size);
      write(fd, buffer, size);
      // printf ("Wrote...\n");
      break;

    case 10: //SEEK

      break;

    case 11: //TELL

      break;

    case 12: //CLOSE

      break;
      
    default: 
      // TODO
      printf ("Syscall not yet defined!\n");
      break;
  }
  // thread_exit ();   //Was here originally, but seems wrong
}



int open (const char *file)
{
  struct thread *t = thread_current ();
  struct file *_file = NULL;
  printf ("In open: %s\n", file);
  _file = filesys_open (file);
  if(_file == NULL)
  {
    // Maybe close it?
    // Most things tried are null, though not null for 'ls ls' ...for some reason
    printf ("Null file!\n");
    return -1;
  }
  else
    printf("Not null!\n");
  // Put it in thread array file_list
  file_index_increment();
  t->file_list[t->file_index] = palloc_get_page(0);
  strlcpy (t->file_list[t->file_index], file, PGSIZE);
  // memcpy (t->file_list[t->file_index], file, strlen (file + 1));
  t->file_index++;
  printf ("Returning fd: %d, file name = %s\n", t->file_index + 1, t->file_list[t->file_index - 1]);
  return t->file_index + 1; // -1 + 2 GO BACK AND FIX THIS
}

int write (int fd, const void *buffer, unsigned size)
{
  char *file = palloc_get_page(0);
  //Get file name from fd
  if (fd >= 2) {
    fd -= 2;
    struct thread *t = thread_current();
    strlcpy (file, t->file_list[fd], PGSIZE);
    printf("File name: %s", t->file_list[fd]);
  }

  //Special case for writing to console
  if (fd == 1) {
    putbuf((char *)buffer, size);
  }
  return 0;
}
