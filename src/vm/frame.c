#include "vm/frame.h"
#include "threads/malloc.h"
#include <stdio.h>
#include "lib/kernel/list.h"
#include <stdint.h>
#include "threads/palloc.h"
#include "threads/thread.h"

// static struct frame_table *ftb;						// Stores the one and only frame table

/* Initialize this file */
void frame_table_init () {

	//Initially set all members of frame table to null char
	int i = 0;
	uint32_t* begin_frame;
	while((begin_frame = (uint32_t*) palloc_get_page(PAL_USER | PAL_ZERO)) != NULL) {
		frames[i].occupier = begin_frame;
		frames[i].t = thread_current();
		i++;

	}

	free_frames = NUM_FRAMES;

	//test_frame_table(i);

}

void test_frame_table(int max){
	int i;
	for(i = 0; i < max; i++){
		printf("frames[%d].occupier = %p    frames[%d].t->name = %s\n",i,frames[i].occupier,
			i,frames[i].t->name);
	}
}
// struct frame * frame_create () {
// 	struct frame *f = (struct frame*)malloc ( sizeof (struct frame));
// 	list_push_back (&(ftb->frames), &(f->frame_elem));
// 	return f;
// }

/*struct frame * frame_delete () {

}

struct frame * frame_evict () {

}*/


/* Returns a pointer to a free page */
uint32_t * get_frame () {
	uint32_t* begin_frame = NULL;
	int i;
	if(free_frames == 0) {
		printf ("Out of frames!\n");
		exit(0);
	}
	for(i = 0; i < NUM_FRAMES; i++) {
		if(*frames[i].occupier == 0){
			free_frames--;
			begin_frame = frames[i].occupier;
			frames[i].t = thread_current();
			break;
		}
	}

	//test_frame_table(NUM_FRAMES);

	return begin_frame;
}









