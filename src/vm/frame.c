#include "vm/frame.h"
#include "threads/malloc.h"
#include <stdio.h>
#include "lib/kernel/list.h"
#include <stdint.h>
#include "threads/palloc.h"
#include "threads/thread.h"
#include "vm/page.h"

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
		printf("frames[%d].occupier = %p,	 frames[%d].physical = %p,    frames[%d].t->name = %s\n",i,frames[i].occupier,
			i, frames[i].physical, i,frames[i].t->name);
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

bool frame_evict () {
	//printf("in frame evict\n");
	//int* count = t->replace_count;
	//int cnt = *count;
	lock_acquire (thread_current ()->vm_lock); // Acquire the vm_lock
	thread_current ()->holds_vm_lock = true;
	struct thread *t;
	int ass_count = 0;
	uint8_t *upage;
	struct frame replace_frame;
	struct supp_page * p;
	bool go = true;
	while(go){
		replace_frame = frames[replace_count];
		t = replace_frame.t;
		upage = replace_frame.occupier;
		p = search_supp_table(upage);
		// //if((p->is_stack) && ass_count)
		// 	ASSERT(!(p->is_stack))
		// //else if ((p->is_stack))
		// //	ass_count++;
		if(p != NULL && !(p->is_stack)){
			/* We are evicting a non-stack page from frames, and can 
				 safely "throw it away" if its not dirty */
			pagedir_clear_page (t->pagedir, upage);
			p->present = false;
			//ASSERT ( 0 == 1);
			go = false;
		}
		else if(p != NULL && p->is_stack) {
			/* We are evicting a stack page to the swap space, but must 
			   update the supp_page to reflect the ofs into the swap partition
			   so that it may be reloaded into memory in the future */
			p->file = NULL;
			p->ofs = NULL;
			p->read_bytes = 0;
			p->zero_bytes = 0;
			p->present = false;
		}
		else {
			replace_count++;
			if(replace_count >= NUM_FRAMES)
				replace_count = 0; 
		}
	}
	//*count = cnt;
	//printf("replace_count = %d\n",replace_count);
	lock_release (thread_current ()->vm_lock); // Release the vm_lock
	thread_current ()->holds_vm_lock = false;
	return return_frame(upage);
}


/* Returns a pointer to a free frame */
uint32_t * get_frame (uint32_t *occu) {
	uint32_t* temp_frame = NULL;
	int i;
	//lock_acquire (thread_current ()->vm_lock); // Acquire the vm_lock
	if(free_frames == 0) {
		if(!frame_evict())
			exit(-1);
	}
	lock_acquire (thread_current ()->vm_lock); // Acquire the vm_lock
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
	return temp_frame;
}









