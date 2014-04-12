#include "vm/frame.h"
#include "threads/malloc.h"
#include <stdio.h>
#include "lib/kernel/list.h"
#include <stdint.h>
#include "threads/palloc.h"
#include "threads/thread.h"
#include "userprog/syscall.h"

// static struct frame_table *ftb;						// Stores the one and only frame table
int free_frames;				// The max number of frames in the frame table

/* Initialize this file */
void frame_table_init () {
	// Initially set all members of frame table to null char
	int i = 0;
	uint32_t* begin_frame;
	while((begin_frame = (uint32_t*) palloc_get_page(PAL_USER)) != NULL) {
		frames[i].physical = begin_frame;
		frames[i].occupier = NULL;
		frames[i].t = thread_current();
		i++;

	}

	free_frames = NUM_FRAMES;

	//test_frame_table(i);

}

void test_frame_table(int max){
	int i;
	// int j;
	for(i = 0; i < max; i++){
		printf("frames[%d].occupier = %p,	 (frames[%d].physical) = %p,    frames[%d].t->name = %s\n",i,frames[i].occupier,
			i, (frames[i].physical), i, frames[i].t->name); // pagedir_is_dirty(frames[i].t->pagedir,frames[i].occupier));
		// for(j = 0; j < 128; j++)
		// 	printf("Address %p = %p\n",(frames[i].physical)+(4*j), *((frames[i].physical)+(4*j)));
		// printf("\n");
	}
}
// struct frame * frame_create () {
// 	struct frame *f = (struct frame*)malloc ( sizeof (struct frame));
// 	list_push_back (&(ftb->frames), &(f->frame_elem));
// 	return f;
// }

bool return_frame (uint32_t *addr) {
	// Give the frame back!
	// printf("in return_frame\n");
	int i;
	for(i = 0; i < NUM_FRAMES; i++) {
		if(frames[i].occupier == addr){
			frames[i].occupier = NULL;
			frames[i].t = get_initial_thread();
			free_frames++;
			return true;
		}
	}
	return false;
}

bool return_frame_by_tid (tid_t tid){
	bool found = false;
	int i;
	struct thread *thread;
	uint8_t *up;
	for(i = 0; i < NUM_FRAMES; i++) {
		if(frames[i].t->tid == tid){
			up = frames[i].occupier;
			frames[i].occupier = NULL;
			thread = frames[i].t;
			frames[i].t = get_initial_thread();
			free_frames++;
			found = true;
			pagedir_clear_page (thread->pagedir, up);

		}
	}
	return found;
}

bool frame_evict () {
	//printf("in frame evict\n");
	//int* count = t->replace_count;
	//int cnt = *count;
	// lock_acquire (thread_current ()->vm_lock); // Acquire the vm_lock
	// thread_current ()->holds_vm_lock = true;
	struct thread *t;
	uint8_t *upage;
	struct frame replace_frame;
	struct supp_page * p;
	bool go = true;
	bool wrote_to_swap = false;
	bool dirty;
	while(go){
		replace_frame = frames[replace_count];
		t = replace_frame.t;
		upage = replace_frame.occupier;
		p = search_supp_table(upage, t);
		// 	ASSERT(!(p->is_stack))
		// //else if ((p->is_stack))
		// //	ass_count++;
		if(p != NULL){
			dirty = pagedir_is_dirty(t->pagedir, upage);
			if(dirty){
				wrote_to_swap = write_to_swap(upage, p);
			}
			if(!dirty || wrote_to_swap){
				pagedir_clear_page (t->pagedir, upage);
				p->present = false;
				go = false;
			}
		else
			ASSERT ( 0 == 1);
		}
		replace_count++;
		replace_count = replace_count%NUM_FRAMES;
	}
	//*count = cnt;
	//printf("replace_count = %d\n",replace_count);
	// lock_release (thread_current ()->vm_lock); // Release the vm_lock
	// thread_current ()->holds_vm_lock = false;
	return return_frame(upage);
}


/* Returns a pointer to a free frame */
uint32_t * get_frame (uint32_t *occu) {
	uint32_t* temp_frame = NULL;
	int i;
	if(!thread_current()->holds_vm_lock)
		lock_acquire (thread_current ()->vm_lock); // Acquire the vm_lock
    thread_current ()->holds_vm_lock = true;

	if(free_frames == 0) {
		if(!frame_evict()) 
			exit(-1);
	}
	// lock_acquire (thread_current ()->vm_lock); // Acquire the vm_lock
	for(i = 0; i < NUM_FRAMES; i++) {
		if(frames[i].occupier == NULL){
			free_frames--;
			temp_frame = frames[i].physical;
			frames[i].t = thread_current();
			frames[i].occupier = occu;
			break;
		}
	}
	lock_release (thread_current ()->vm_lock); // Release the vm_lock
	thread_current ()->holds_vm_lock = false;
	return temp_frame;
}









