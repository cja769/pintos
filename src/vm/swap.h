#include "devices/block.h"
#include "lib/kernel/list.h"
#include "vm/page.h"

static struct block *swap_block;

struct list free_sector_list;

static int swap_size;

struct sector_group {
	block_sector_t start_sector;

	struct list_elem sectorelem;
};

/* Prototypes */
void swap_init (void);
bool write_to_swap (uint8_t *upage, struct supp_page * p); 
uint8_t * read_from_swap (struct supp_page * p);
