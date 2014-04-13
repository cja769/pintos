//#include "vm/swap.h"
#include "vm/frame.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"

/* Initializes the swap block */
void swap_init () {
	//Samantha drove this method
	swap_block = block_get_role(BLOCK_SWAP);
	swap_size = block_size(swap_block);
	list_init(&free_sector_list);

	//Fill free list with empty sector groups
	int i;
	for (i = 0; i < swap_size; i += (PGSIZE / BLOCK_SECTOR_SIZE)) { // increment i by how many sectors a page will need
		struct sector_group *sgroup = malloc(sizeof (struct sector_group));
		sgroup->start_sector = i;
		list_push_back(&free_sector_list, &(sgroup->sectorelem));
	}
}

/* Writes a page to the swap block */
bool write_to_swap (uint8_t *upage, struct supp_page * p) {
	//Jason drove this method
	int i;
	int j;
	if(swap_size < (PGSIZE / BLOCK_SECTOR_SIZE)) //Not enough room left in swap block
		return false;
	struct sector_group *sg = list_entry(list_pop_front(&free_sector_list), struct sector_group, sectorelem); //Sector group pulled from free list
	block_sector_t start_sector = sg->start_sector; //Sector number that begins the group to which page should be written
	p->sector = start_sector;
	free(sg);
	//Since a sector is smaller than a page, a page must be written to multiple sectors
        //Loop to write all sectors for a page
	for (i = 0, j = 0; i < PGSIZE; i += BLOCK_SECTOR_SIZE, j++) {
		block_write(swap_block, start_sector + j, upage + i);
	}

	swap_size -= PGSIZE / BLOCK_SECTOR_SIZE;
	return true;
}

/* Reads a faulting page from the swap block to physical memory */
uint8_t * read_from_swap (struct supp_page * p) {
	//Dustin and Calvin drove this method
	block_sector_t start_sector = p->sector; //First sector from which this page should be read
	if(start_sector == (unsigned int) -1){
		printf("somehow start_sector is -1\n");
		return NULL;
	}
	int i;
	int j;
	void * upage = palloc_get_page (0);
	if( (int*) upage == NULL){
		printf("The buffer is NULL\n");
		ASSERT( 0==1);
	}
	// Read sectors sequentially
	for (i = 0, j = 0; i < PGSIZE; i += BLOCK_SECTOR_SIZE, j++) {
		block_read(swap_block, start_sector + j, upage + i); 
	}
        //Return the sector group to the free list of sectors, clears its contents
	struct sector_group *sgroup = malloc(sizeof (struct sector_group));
	sgroup->start_sector = start_sector;
	list_push_back(&free_sector_list, &(sgroup->sectorelem));
	p->sector = (unsigned int) -1;
	swap_size += (PGSIZE / BLOCK_SECTOR_SIZE);
	return (uint8_t*) upage;
}
