#include "vm/frame.h"
#include "threads/malloc.h"
#include <stdio.h>
#include "lib/kernel/list.h"
#include <stdint.h>
#include "threads/palloc.h"
#include "threads/thread.h"

// static struct frame_table *ftb;						// Stores the one and only frame table
int free_frames;				// The max number of frames in the frame table

/* Initialize this file */
void frame_table_init () {

	//Initially set all members of frame table to null char
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

bool return_frame (uint32_t addr) {
	// Give the frame back!
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

/*struct frame * frame_evict () {

}*/


/* Returns a pointer to a free page */
uint32_t * get_frame (uint32_t occu) {
	uint32_t* temp_frame = NULL;
	int i;
	if(free_frames == 0) {
		printf ("Out of frames!\n");
		exit(0);
	}
	for(i = 0; i < NUM_FRAMES; i++) {
		if(frames[i].occupier == NULL){
			if (occu == NULL)
				occu = frames[i].physical;
			free_frames--;
			temp_frame = frames[i].physical;
			frames[i].t = thread_current();
			frames[i].occupier = occu;
			break;
		}
	}

	return temp_frame;
}









