#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
//#include "vm/page.h"
#include "vm/frame.h"
#include "threads/malloc.h"


static thread_func start_process NO_RETURN;
static bool load (struct args *file_name, void (**eip) (void), void **esp);

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
  // Dustin and Jason drove this method
  /* A struct to store the command line and number of
     arguments for each new process */
  struct args *arguments = palloc_get_page(0);
  struct thread *t = thread_current();
  struct list_elem *e;

  char *fn_copy;
  tid_t tid;

  char *saveptr;// /* Used for strtok_r */
  arguments->argc = 0;

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);
  char *temp = palloc_get_page(0); 
  temp = strtok_r(fn_copy, " ", &saveptr);
  arguments->argv = palloc_get_page(0);
  arguments->argv[arguments->argc] = palloc_get_page(0);
  if (arguments->argv[arguments->argc] == NULL || arguments->argv == NULL)
    return TID_ERROR;
  strlcpy (arguments->argv[arguments->argc], temp, PGSIZE);
  arguments->argc++;
  arguments->argv[arguments->argc] = palloc_get_page(0);
  while((temp = strtok_r(NULL, " ", &saveptr)) != NULL){ /* Parse the command line and store them in args */
    arguments->argv[arguments->argc] = palloc_get_page(0);
    if (arguments->argv[arguments->argc] == NULL)
      return TID_ERROR;
    strlcpy (arguments->argv[arguments->argc], temp, PGSIZE);
    arguments->argc++; // Increment the number of args
  }  

  /* Create a new thread to execute FILE_NAME. */
  // printf("filen_name before create = %s\n", file_name);
  tid = thread_create (file_name, PRI_DEFAULT, start_process, arguments);
  /* The parent thread running this method will wait for exec to finish
   If the child process failed to load, this method returns -1 */
  sema_down(&thread_current()->exec_sema); 
  for (e = list_begin (&t->children); e != list_end (&t->children);
       e = list_next (e))
    {
      struct thread *copy = list_entry (e, struct thread, elem);
      if (copy->tid == tid && copy->exit_status == -1)
        tid = -1;
    }
  if (tid == TID_ERROR) {
    palloc_free_page (fn_copy); 
  }
  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
  // Dustin and Jason drove this method
  //used to loop through children list
  struct thread *t = thread_current();
  struct list_elem *e;

  // Create the struct args and store the command line 
  struct args *arguments = (struct args*)file_name_;
  char *file_name = arguments->argv[0];
  struct intr_frame if_;
  bool success;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (arguments, &if_.eip, &if_.esp);

  /* If load failed, quit and set the parents copy of this threads exit status to 0, 
     otherwise allow the parent to continue after exec by calling sema_up */
  palloc_free_page (file_name);
  if (!success) 
  {
    for (e = list_begin (&t->parent->children); e != list_end (&t->parent->children);
       e = list_next (e))
    {
      struct thread *copy = list_entry (e, struct thread, elem);
      if (copy->tid == thread_current()->tid)
        copy->exit_status = -1;
    }
    sema_up(&thread_current()->parent->exec_sema);
    thread_exit ();
  }
  sema_up(&thread_current()->parent->exec_sema);
  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid) 
{
  // Jason drove this method
  struct thread *t = thread_current ();

  struct list_elem *e;
  int exit_status;

  /* Iterate through the parents list of children and find a match with child_tid.
     If a child is found and has already terminated, remove the thread from this threads list of
   children and return with its exit status. Otherwise, call sema_down to wait for the
   child process to terminate or call sema_up. */
  for (e = list_begin (&t->children); e != list_end (&t->children);
       e = list_next (e))
    {
      struct thread *copy = list_entry (e, struct thread, elem);
      if (copy->tid == child_tid)
      {
        while (copy->status != THREAD_DYING)
        {
          sema_down(&copy->mutex);
        }

        ASSERT (copy->status == THREAD_DYING); 
        exit_status = copy->exit_status;
        list_remove (e);
        return exit_status;
      } // break
    }
  
  // If control reaches here, the thread child_tid does not exist
  return -1;
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;
  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp, struct args *file_name);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_supp_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (struct args *file_name, void (**eip) (void), void **esp) 
{
  // Dustin drove this method
  char *file_ = *file_name->argv;
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  /* Initialize the supplemental page table */
  supp_page_table_init ();

  /* Open executable file. */
  file = filesys_open (file_);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_);
      goto done; 
    }
  file_deny_write (file); /* Sets deny write to files inode */

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_supp_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp, file_name))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */

  thread_current ()->exe = file;
  return success;
}


/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);
  bool file_null = file == NULL;
  if(!file_null){
    file_seek (file, ofs);
  }
  while ((read_bytes > 0 || zero_bytes > 0) && !file_null) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      uint8_t *kpage = get_frame(upage); //Our method, gets frame of physical memory
      if (kpage == NULL){
       return false;
      }
      /* Load this page. */
      /* Jason and Calvin drove here for lock stuff */
      bool had_lock = thread_current()->holds_vm_lock;
      if(!had_lock){
        lock_acquire (thread_current ()->vm_lock); // Acquire the vm_lock
        thread_current ()->holds_vm_lock = true;
      }
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          if(!had_lock){
            lock_release (thread_current ()->vm_lock); // Release the vm_lock
            thread_current ()->holds_vm_lock = false;
          }

          return_frame (upage); //Invalid read, must return frame of phys memory
          return false; 
        }
      if(!had_lock){
        lock_release (thread_current ()->vm_lock); // Release the vm_lock
        thread_current ()->holds_vm_lock = false;
      }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable)) 
        {
          return_frame (upage); //Our method, returns frame that was acquired earlier
          return false; 
        }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}


/* load_supp_segment - Loads information about a page into the supplemental page table
   that will be used later to load the page into physical memory and the page directory */
static bool
load_supp_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  //Dustin and Samantha drove this method
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);
  off_t new_ofs = ofs;
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;
      load_supp_page(file,new_ofs, (void *) upage, page_read_bytes, page_zero_bytes, writable, false); 
      new_ofs += (page_read_bytes + page_zero_bytes);
      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp, struct args *file_name) 
{
  /* Dustin and Jason drove this method */
  uint8_t *kpage;
  bool success = false;
  int i;
  char *myesp;
  int addr[file_name->argc]; /* To store the address of each argument we push on the stack */
  int addr_argv; /* To store the address of the first pointer on the stack */
  uint8_t *upage = ((uint8_t *) PHYS_BASE) - PGSIZE;
  kpage = get_frame(upage);
  if (kpage != NULL) 
    {
      success = install_page (upage, kpage, true);
      if (success)
      {
        load_supp_page(NULL, 0, upage, 0, 0, true, true);
        // test_supp_page_table();
        *esp = PHYS_BASE;
        thread_current()->esp = (uint8_t *)PHYS_BASE - PGSIZE; /*For stack growth - need
          bottom of stack page to create new page*/
        myesp = (char*)*esp; // Modify a copy of myesp for easier arithmetic, and set esp at the end
        /* Pushes command line onto the stack from right to left */
        for(i = file_name->argc-1; i >= 0; i--)
        {
          myesp -= (strlen(file_name->argv[i]) + 1);
          addr[i] = (int)myesp; /* Save the address of each argument on the stack */
          strlcpy(myesp, file_name->argv[i], strlen(file_name->argv[i]) + 1);
        }

    // Jason drove here
        myesp = (char*)ROUND_DOWN((uintptr_t)myesp, 4); /* word-align */
        myesp -= 4;

    // Dustin drove here
        /* Pushes addresses of arguments onto the stack */
        for(i = file_name->argc-1; i >= 0; i--)
        {
          myesp -= 4;
          memcpy(myesp, &addr[i], sizeof(int));
          if(i == 0)
            memcpy(&addr_argv, &myesp, sizeof(int));
        }
        myesp -= 4;
        memcpy(myesp, &addr_argv, sizeof(int *)); /* Pushes the address of argv onto the stack */
        myesp -= 4;
        memcpy(myesp, &(file_name->argc), sizeof(int)); /* Pushes argc onto the stack */
        myesp -= 4;
        i = 0;
        memcpy(myesp, &i, sizeof(char *)); /* Fake return address */
        *esp = myesp;
      }
      else
        return_frame (((uint8_t *) PHYS_BASE) - PGSIZE);
    }
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}
