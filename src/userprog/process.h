#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

/* A struct to store the command line, parsed, and
   the number of arguments. */
typedef struct args {
  char ** argv;
  int argc;
}args;

#endif /* userprog/process.h */