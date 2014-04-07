#include "vm/page.h"
#include "threads/malloc.h"
#include <stdio.h>
#include "lib/kernel/list.h"
#include <stdint.h>
#include "threads/palloc.h"
#include "threads/thread.h"

/* Initialize this supplemental page table */
void supp_page_table_init () {
	list_init(&thread_current ()->supp_page_table);
}

/* Rather than loading a segment into a user page, record the segment of the
   executable into the threads supplemental page table for when the thread 
   page faults later? */
bool load_supp_page(struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	printf("Herp\n");
	struct supp_page *p = malloc (sizeof (struct supp_page));
	p->file = file;
	p->ofs = ofs;
	p->upage = upage;
	p->read_bytes = read_bytes;
	p->zero_bytes = zero_bytes;
	p->writable = writable;
  p->present = false;
	list_push_back(&thread_current ()->supp_page_table, &p->suppelem);
	test_supp_page_table();
	return true;
}

void test_supp_page_table() {
	struct list_elem *e;

      for (e = list_begin (&thread_current()->supp_page_table); e != list_end(&thread_current()->supp_page_table); e = list_next(e))
        {
            struct supp_page *p = list_entry (e, struct supp_page, suppelem);
          	printf ("Supp page!\n");
        }
}