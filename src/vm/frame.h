#include "lib/kernel/list.h"
#include <stdint.h>
#include "vm/swap.h"

#define NUM_FRAMES 383


struct frame {
	uint32_t *occupier;     				// The address of the occupying page      
	uint32_t *physical;  					// The address of the frame page  
	struct thread *t;					// The thread that has this frame
	int * replace_count;					// A pointer to the index of the next frame to evict random comment delete later
};

struct frame frames[NUM_FRAMES]; 				//Frame table


uint32_t * get_frame (uint32_t *occu);
bool frame_evict (void);
struct frame * frame_create (void);
void test_frame_table(int max);
void frame_table_init (void);
bool return_frame (uint32_t *addr);
bool return_frame_by_tid (tid_t tid);
