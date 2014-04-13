#include "vm/frame.h"
#include "threads/malloc.h"
#include <stdio.h>
#include "lib/kernel/list.h"
#include <stdint.h>
#include "threads/palloc.h"
#include "threads/thread.h"
#include "userprog/syscall.h"

int free_frames;				// The max number of frames in the frame table

/* Initialize the frame table, to be used for all processes */
void frame_table_init () {
        //Samantha drove this method
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


/* Prints out contents of frame table for debugging purposes - not currently used */
void test_frame_table(int max){
        //Dustin drove this method
	int i;
	for(i = 0; i < max; i++){
		printf("frames[%d].occupier = %p,	 (frames[%d].physical) = %p,    frames[%d].t->name = %s\n",i,frames[i].occupier,
			i, (frames[i].physical), i, frames[i].t->name);
	}
}


/* Frees a frame from physical memory */
bool return_frame (uint32_t *addr) {
        //Dustin drove this method
        //Loops through frame table to find frame that matches the given address, then clears that frame
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

/* Frees a frame from physical memory, used when freeing resources when a thread dies */
bool return_frame_by_tid (tid_t tid){
        //Calvin and Jason drove this method
	bool found = false;
	int i;
	struct thread *thread;
	uint8_t *up;
	//Loops through frame table to find frame whose thread matches the given thread, clears that frame
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

/* Evicts a frame once memory is full following a FIFO replacement policy 
   Also writes to swap if page in frame has been altered (is dirty) */
bool frame_evict () { 
    //Calvin, Jason, and Samantha drove this method	
    struct thread *t;
    uint8_t *upage;
    uint8_t *kpage;
    struct frame replace_frame;
    struct supp_page * p;
    bool go = true;
    bool wrote_to_swap = false;
    bool dirty;
    //Find next frame to evict, will go through multiple frames if first victim is dirty and could not be written to swap
    while(go){
        replace_frame = frames[replace_count]; //replace_count is the index of the next frame to evict
        t = replace_frame.t;
        upage = replace_frame.occupier;
        kpage = replace_frame.physical;
        p = search_supp_table(upage, t);
        if(p != NULL){ //Page is in supplemental page table
            dirty = pagedir_is_dirty(t->pagedir, upage);
            if(dirty){ //Page has been altered and needs to be written to swap
                wrote_to_swap = write_to_swap(kpage, p); 
            }
            if(!dirty || wrote_to_swap){ //Page has not been altered or has been successfully written to swap
		//Can just clear page
                pagedir_clear_page (t->pagedir, upage);
                p->present = false;
                go = false;
            }
        }
        replace_count++;
        replace_count = replace_count%NUM_FRAMES; //To wrap around frame table of replace_count becomes too large
    }
    return return_frame(upage);
}

/* Returns a pointer to a free frame */
uint32_t * get_frame (uint32_t *occu) {
        //Jason drove this method
	uint32_t* temp_frame = NULL;
	int i;
	bool had_lock = thread_current()->holds_vm_lock;
	if(!had_lock) {
		lock_acquire (thread_current ()->vm_lock); // Acquire the vm_lock
  		thread_current ()->holds_vm_lock = true;
	}

	if(free_frames == 0) { //Physical memory is full
		if(!frame_evict()) //Evicts frame to later be filled 
			exit(-1); //Exit if cannot evict for some reason
	}
	//Loop through frame table to find index of a free frame
	for(i = 0; i < NUM_FRAMES; i++) { 
		if(frames[i].occupier == NULL){
			free_frames--;
			temp_frame = frames[i].physical;
			frames[i].t = thread_current();
			frames[i].occupier = occu;
			break;
		}
	}
	if(!had_lock) {
		lock_release (thread_current ()->vm_lock); // Release the vm_lock
		thread_current ()->holds_vm_lock = false;
	}
	return temp_frame;
}









