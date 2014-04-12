//#include "vm/swap.h"
#include "vm/frame.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"

void swap_init () {
	swap_block = block_get_role(BLOCK_SWAP);
	swap_size = block_size(swap_block);
	list_init(&free_sector_list);

	//Fill free list with empty sector groups
	int i;
	for (i = 0; i < swap_size; i += (PGSIZE / BLOCK_SECTOR_SIZE)) {
		struct sector_group *sgroup = malloc(sizeof (struct sector_group));
		sgroup->start_sector = i;
		list_push_back(&free_sector_list, &(sgroup->sectorelem));
	}
}

bool write_to_swap (uint8_t *upage, struct supp_page * p) {
	int i;
	int j;
	//printf("swap_size = %d\n",swap_size);
	if(swap_size < (PGSIZE / BLOCK_SECTOR_SIZE))
		return false;
	struct sector_group *sg = list_entry(list_pop_front(&free_sector_list), struct sector_group, sectorelem);
	block_sector_t start_sector = sg->start_sector;
	p->sector = start_sector;
	free(sg);
//	printf("sector: %d\n", start_sector);
	//printf("thread_current has vm lock == %d in read_from swap\n", thread_current()->holds_vm_lock);
	//lock_acquire (thread_current ()->vm_lock); // Acquire the vm_lock
	//thread_current ()->holds_vm_lock = true;
	for (i = 0, j = 0; i < PGSIZE; i += BLOCK_SECTOR_SIZE, j++) {
		printf("writing to swap pass %d\n",j);
		// printf("upage = %p\n",upage + i);
		block_write(swap_block, start_sector + j, upage + i); //Aaggghhh pointer stuff
		printf("back from writing to swap pass %d\n",j);
	}

	swap_size -= PGSIZE / BLOCK_SECTOR_SIZE;
	//lock_release (thread_current ()->vm_lock); // Release the vm_lock
	//thread_current ()->holds_vm_lock = false;
	return true;
}

uint8_t * read_from_swap (struct supp_page * p) {
	block_sector_t start_sector = p->sector;
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
	//printf("thread_current has vm lock == %d in read_from swap\n", thread_current()->holds_vm_lock);
	for (i = 0, j = 0; i < PGSIZE; i += BLOCK_SECTOR_SIZE, j++) {
		block_read(swap_block, start_sector + j, upage + i); //Aaggghhh pointer stuff
	}
	struct sector_group *sgroup = malloc(sizeof (struct sector_group));
	sgroup->start_sector = start_sector;
	list_push_back(&free_sector_list, &(sgroup->sectorelem));
	p->sector = (unsigned int) -1;
	swap_size += (PGSIZE / BLOCK_SECTOR_SIZE);
	return (uint8_t*) upage;
}
