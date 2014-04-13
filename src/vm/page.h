#include "lib/kernel/list.h"
#include <stdint.h>
#include <inttypes.h>
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
#include "devices/block.h"

struct supp_page {
  //Information necessary to load page into physical memory
  struct file *file;
  off_t ofs;
  uint8_t *upage;
  uint32_t read_bytes;
  uint32_t zero_bytes;

  block_sector_t sector;			// The first sector in the swap block to which this page's info is written
  bool writable;				// If page is writable
  bool present;					// If page is present in physical memory
  bool is_stack;				// If page is a stack page

  struct list_elem suppelem;			// List element for supplemental page table
};

/* Prototypes */
void supp_page_table_init (void);
struct supp_page * search_supp_table(uint8_t *upage, struct thread* t);
bool load_supp_page(struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable, bool is_stack);
void test_supp_page_table(void);
