#include "userprog/exception.h"
#include <inttypes.h>
#include <stdio.h>
#include "userprog/gdt.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/syscall.h"
#include "userprog/process.h"
#include "vm/frame.h"
//#include "vm/page.h"
#include "threads/pte.h"

/* Number of page faults processed. */
static long long page_fault_cnt;

static void kill (struct intr_frame *);
static void page_fault (struct intr_frame *);

/* Registers handlers for interrupts that can be caused by user
   programs.

   In a real Unix-like OS, most of these interrupts would be
   passed along to the user process in the form of signals, as
   described in [SV-386] 3-24 and 3-25, but we don't implement
   signals.  Instead, we'll make them simply kill the user
   process.

   Page faults are an exception.  Here they are treated the same
   way as other exceptions, but this will need to change to
   implement virtual memory.

   Refer to [IA32-v3a] section 5.15 "Exception and Interrupt
   Reference" for a description of each of these exceptions. */
void
exception_init (void) 
{
  /* These exceptions can be raised explicitly by a user program,
     e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
     we set DPL==3, meaning that user programs are allowed to
     invoke them via these instructions. */
  intr_register_int (3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
  intr_register_int (4, 3, INTR_ON, kill, "#OF Overflow Exception");
  intr_register_int (5, 3, INTR_ON, kill,
                     "#BR BOUND Range Exceeded Exception");

  /* These exceptions have DPL==0, preventing user processes from
     invoking them via the INT instruction.  They can still be
     caused indirectly, e.g. #DE can be caused by dividing by
     0.  */
  intr_register_int (0, 0, INTR_ON, kill, "#DE Divide Error");
  intr_register_int (1, 0, INTR_ON, kill, "#DB Debug Exception");
  intr_register_int (6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
  intr_register_int (7, 0, INTR_ON, kill,
                     "#NM Device Not Available Exception");
  intr_register_int (11, 0, INTR_ON, kill, "#NP Segment Not Present");
  intr_register_int (12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
  intr_register_int (13, 0, INTR_ON, kill, "#GP General Protection Exception");
  intr_register_int (16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
  intr_register_int (19, 0, INTR_ON, kill,
                     "#XF SIMD Floating-Point Exception");

  /* Most exceptions can be handled with interrupts turned on.
     We need to disable interrupts for page faults because the
     fault address is stored in CR2 and needs to be preserved. */
  intr_register_int (14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/* Prints exception statistics. */
void
exception_print_stats (void) 
{
  printf ("Exception: %lld page faults\n", page_fault_cnt);
}

/* Handler for an exception (probably) caused by a user process. */
static void
kill (struct intr_frame *f) 
{
  /* This interrupt is one (probably) caused by a user process.
     For example, the process might have tried to access unmapped
     virtual memory (a page fault).  For now, we simply kill the
     user process.  Later, we'll want to handle page faults in
     the kernel.  Real Unix-like operating systems pass most
     exceptions back to the process via signals, but we don't
     implement them. */
     
  /* The interrupt frame's code segment value tells us where the
     exception originated. */
  switch (f->cs)
    {
    case SEL_UCSEG:
      /* User's code segment, so it's a user exception, as we
         expected.  Kill the user process.  */
      printf ("%s: dying due to interrupt %#04x (%s).\n",
              thread_name (), f->vec_no, intr_name (f->vec_no));
      intr_dump_frame (f);
      thread_exit (); 

    case SEL_KCSEG:
      /* Kernel's code segment, which indicates a kernel bug.
         Kernel code shouldn't throw exceptions.  (Page faults
         may cause kernel exceptions--but they shouldn't arrive
         here.)  Panic the kernel to make the point.  */
      intr_dump_frame (f);
      PANIC ("Kernel bug - unexpected interrupt in kernel"); 

    default:
      /* Some other code segment?  Shouldn't happen.  Panic the
         kernel. */
      printf ("Interrupt %#04x (%s) in unknown segment %04x\n",
             f->vec_no, intr_name (f->vec_no), f->cs);
      thread_exit ();
    }
}

/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to project 2 may
   also require modifying this code.

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
static void
page_fault (struct intr_frame *f) 
{
  bool not_present;  /* True: not-present page, false: writing r/o page. */
  bool write;        /* True: access was write, false: access was read. */
  bool user;         /* True: access by user, false: access by kernel. */
  void *fault_addr;  /* Fault address. */

  /* Obtain faulting address, the virtual address that was
     accessed to cause the fault.  It may point to code or to
     data.  It is not necessarily the address of the instruction
     that caused the fault (that's f->eip).
     See [IA32-v2a] "MOV--Move to/from Control Registers" and
     [IA32-v3a] 5.15 "Interrupt 14--Page Fault Exception
     (#PF)". */
  asm ("movl %%cr2, %0" : "=r" (fault_addr));

  /* Turn interrupts back on (they were only off so that we could
     be assured of reading CR2 before it changed). */
  intr_enable ();

  /* Count page faults. */
  page_fault_cnt++;

  /* Determine cause. */
  not_present = (f->error_code & PF_P) == 0;
  write = (f->error_code & PF_W) != 0;
  user = (f->error_code & PF_U) != 0;

  /* Samantha drove here */
  /* If a user tries to load and dereference a null pointer, exit */
  if (user && (!is_user_vaddr(fault_addr) || fault_addr == NULL))
    exit(-1);

  bool dont_kill = false; // true or false if we are loading this faulting address THIS time, formerly called loaded

  //printf("\n\n\nBegin page_fault: \n");
//  test_frame_table(10);
  //test_supp_page_table();

  //printf("fault address: %p, f->eip: %p\n", fault_addr, f->eip);
 // printf("fault address: %p\n", fault_addr);


  struct list_elem *e;
 
  for (e = list_begin (&thread_current()->supp_page_table); e != list_end(&thread_current()->supp_page_table) && !dont_kill; e = list_next(e))
    {
        struct supp_page *p = list_entry (e, struct supp_page, suppelem);
        if (pt_no(fault_addr) == pt_no(p->upage) && p->present == false) {
          // printf("fault_addr: %08X, p->ofs: %08X, p->read_bytes: %08X, p->upage: %08X, p->zero_bytes: %08X\n", 
            // fault_addr, p->ofs, p->read_bytes, p->upage, p->zero_bytes);
          // printf("fault pt_no: %d, upage pt_no: %d\n", pt_no(fault_addr), pt_no(p->upage));

          if(p->sector == (unsigned int) -1){ 
           // printf("page faulted and reading from file\n");
            //printf("about to acquire lock\n");
            // lock_acquire (thread_current ()->vm_lock); // Acquire the vm_lock
            // thread_current ()->holds_vm_lock = true;
            //printf("lock acquired\n");
            dont_kill = load_segment(p->file, p->ofs, p->upage, p->read_bytes, p->zero_bytes, p->writable);
            //printf("about to release lock\n");
            // lock_release (thread_current ()->vm_lock); // Release the vm_lock
            // thread_current ()->holds_vm_lock = false;
            //printf("lock released\n");
          }
          else{
           // printf("reading from swap\n");
            lock_acquire (thread_current ()->vm_lock); // Acquire the vm_lock
            thread_current ()->holds_vm_lock = true;
            uint8_t * start = read_from_swap(p);
            lock_release (thread_current ()->vm_lock); // Release the vm_lock
            thread_current ()->holds_vm_lock = false;
            if(start != NULL){
              uint8_t *kpage = get_frame(p->upage);
             // printf("upage = %p, kpage = %p, and start = %p\n",p->upage,kpage,start);
              memcpy(kpage,start,PGSIZE);
              palloc_free_page((void*)start);
              dont_kill = install_page(p->upage,kpage,p->writable);
              //printf("*kpage = %p\n",*kpage);
              if(dont_kill)
                pagedir_set_dirty (thread_current()->pagedir,p->upage,true);

            }
            else{
              printf("start is null for some reason\n");
            }
          }

          // printf("load_segment returned %s\n", dont_kill ? "true" : "false");
        if(dont_kill){
          p->present = true;
        }
        // else
          // printf("dont_kill is stil false for some reason\n");
        }
        // else if (pt_no(fault_addr) == pt_no(p->upage) && p->present == true) {    //Commenting out because it may not be necessary
          // dont_kill = true;
        // printf("fault address: %p, *fault_address: %d\n", fault_addr, *(int *)fault_addr);
    }
    

      //Stack growth heuristic

    // printf("stack access, stack: %p, fault: %p, difference: %u\n", f->esp, fault_addr, (unsigned)fault_addr - (unsigned)f->esp);
  if ((unsigned)fault_addr - (unsigned)f->esp <= 32 || (unsigned)f->esp - (unsigned)fault_addr <= 32) {
    // printf("stack access, stack: %p, fault: %p, difference: %u\n", f->esp, fault_addr, (unsigned)fault_addr - (unsigned)f->esp);
    // printf("stack access, stack: %p, fault: %p, difference: %u\n", f->esp, fault_addr, (unsigned)f->esp - (unsigned)fault_addr);
    // printf("page stack stuff %p\n", (PHYS_BASE) - PGSIZE);
    // printf("(unsigned)fault_addr - (unsigned)f->esp %d\n", (unsigned)thread_current()->esp - (unsigned)fault_addr);
      uint8_t *kpage = get_frame(((uint8_t *) thread_current()->esp) - PGSIZE);
      bool success = install_page (((uint8_t *) thread_current()->esp) - PGSIZE, kpage, true);
      dont_kill = success;
      load_supp_page(NULL, 0, ((uint8_t *) thread_current()->esp), 0, 0, true, true);
      thread_current()->esp -= PGSIZE;
  }
  else if (!dont_kill){
    //ASSERT(0==1);
    //printf("fault address: %p\n", fault_addr);
    //test_frame_table(383);
    //printf("execption.c\n");
    exit(-1);
  }
}


