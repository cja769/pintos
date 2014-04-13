#include "devices/block.h"
#include "lib/kernel/list.h"
#include "vm/page.h"

static struct block *swap_block;			//Entire swap block

struct list free_sector_list;				//List of free sectors in swap block

static int swap_size;					//Remaining space in swap block

struct sector_group {					//Wraps up a page-sized group of sectors
	block_sector_t start_sector;			//First sector in group to which a page can be written

	struct list_elem sectorelem;			//List element for free list of sectors
};

/* Prototypes */
void swap_init (void);
bool write_to_swap (uint8_t *upage, struct supp_page * p); 
uint8_t * read_from_swap (struct supp_page * p);
