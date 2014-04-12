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
  struct file *file;
  off_t ofs;
  uint8_t *upage;
  uint32_t read_bytes;
  uint32_t zero_bytes;
  block_sector_t sector;
  bool writable;
  bool present;
  bool is_stack;

  struct list_elem suppelem;
};

/* Prototypes */
void supp_page_table_init (void);
struct supp_page * search_supp_table(uint8_t *upage, struct thread* t);
bool load_supp_page(struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable, bool is_stack);
void test_supp_page_table(void);
