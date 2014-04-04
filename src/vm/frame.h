#include "lib/kernel/list.h"
#include <stdint.h>

#define NUM_FRAMES 383

static int free_frames;				// The max number of frames in the frame table

// /* Frame Table Structure  -- may not need this, can put list somewhere else */
// struct frame_table {
	// MIGHT USE ARRAY, EASIER IF WE CAN (MORE EFFIECIENT)
	// int size;
// };

struct frame {
	uint32_t *occupier;     				// The address of the occupying page      
	uint32_t *physical;  					// The address of the frame page  
	struct thread *t;
};

struct frame frames[NUM_FRAMES];

/* Returns a free page */
uint32_t * get_frame (uint32_t occu);
struct frame * frame_evict (void);
struct frame * frame_create (void);
void test_frame_table(int max);
void frame_table_init (void);
struct frame * frame_delete (void);
