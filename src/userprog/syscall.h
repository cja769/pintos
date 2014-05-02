#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stdbool.h>

/* Process identifier, from /lib/user/syscall.h. */
typedef int pid_t;
#define PID_ERROR ((pid_t) -1)

/* Method prototypes for syscall.c and Pintos sys_calls,
   copied from project description */
int get_arg (int *esp, bool is_pointer);
void syscall_init (void);
void halt (void);
void exit (int status);
pid_t exec (const char *cmd_line);
int wait (pid_t pid);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size); 
int write (int fd, const void *buffer, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);
bool chdir(const char *dir);
bool mkdir(const char *dir);
bool readdir(int fd, char *name);
bool isdir(int fd);
int inumber(int fd);
#endif /* userprog/syscall.h */
