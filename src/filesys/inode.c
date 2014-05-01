#include "filesys/inode.h"
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include <stdio.h>

static uint32_t id_count;

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44


/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  if (pos < inode->data.length)
    return inode->data.start + pos / BLOCK_SECTOR_SIZE;
  else
    return -1;
}

static block_sector_t *
get_ptr_for_sector (struct inode_disk *inode_disk, off_t pos)
{
  ASSERT (inode_disk != NULL);
  //printf("\tpos: %d, length: %d\n", pos, inode_disk->length);
  if (pos <= inode_disk->length)
  {
    //printf("pos / BLOCK_SECTOR_SIZE %d\n", pos / BLOCK_SECTOR_SIZE);
    if (pos / BLOCK_SECTOR_SIZE < 10)
    {
    //  printf("\tdirect block\n");
      //printf("Small file\n");
      /* Our pos is within the first 10 direct blocks */
    //  printf("\tsector pos: %d\n", pos/BLOCK_SECTOR_SIZE);
      return &(inode_disk->start[pos / BLOCK_SECTOR_SIZE]);
    }
    else if (pos /BLOCK_SECTOR_SIZE < 138)
    {
    //  printf("\tindirect block\n");
      /* Our pos is within the first level of indirection */
      // printf("ib0_sector: %\n");
      block_read(fs_device, inode_disk->ib0_sector, inode_disk->ib_0);
      //printf("After getting to indirect block\n");
      int index = (pos / BLOCK_SECTOR_SIZE) - 10;
    //  printf("\tindex in array is %d, pos is: %d, BLOCK_SECTOR_SIZE is: %d\n", index, pos, BLOCK_SECTOR_SIZE);
      //printf("Accessing index in array: %d\n", blah->blocks[1]);
      // if ((inode_disk->ib_0)->blocks[index] == 0 || (inode_disk->ib_0)->blocks[index] < 0 || (inode_disk->ib_0)->blocks[index] > 16384) {// FIX THIS LATER, Maybe revisit zeroing things
      //  free_map_allocate(1, &((inode_disk->ib_0)->blocks[index]));  // Allocate sector for a direct block in the first indirect block if it doesn't exist
      //  printf("just allocated some indirect block\n");  
      // }
      //printf("\tsector at index in indirect block: %d\n", (inode_disk->ib_0)->blocks[index]);
      return &((inode_disk->ib_0)->blocks[index]); 
    }
    else if (pos / BLOCK_SECTOR_SIZE < 16522 )
    {
      //printf("\tdoubly indirect block\n");
      block_read(fs_device, inode_disk->ib1_sector, inode_disk->ib_1);
      int first_index = ((pos / BLOCK_SECTOR_SIZE) - 138) / 128;
      struct inode_indirect_block * indirect = palloc_get_page(0);
      block_read(fs_device, inode_disk->ib_1->blocks[first_index], indirect);
      //printf("\t\t\tinode_disk->ib_1->blocks[first_index] = %d\n",inode_disk->ib_1->blocks[first_index]);
      /* Our pos is within the second level of indirection... uhh... */
      int second_index = ((pos / BLOCK_SECTOR_SIZE) - 138) % 128;
      block_sector_t return_num = indirect->blocks[second_index];
      //printf("return_num = %d\n",return_num);
      // if(indirect != NULL)
      //   free(indirect);
      // else
      //   return -1;
      return &indirect->blocks[second_index]; // TODO
    }
    printf("\tinside first if\n");
    return -1; // Something went wrong!
  }
  else {
    printf("\toutside first if\n");
    return -1;
  }
} 

static block_sector_t
get_sector (struct inode_disk *inode_disk, off_t pos)
{
  block_sector_t * ptr = get_ptr_for_sector(inode_disk, pos);
  //printf("index pointer: %p, sector: %d\n", ptr, *ptr);
  if (ptr != -1) 
    return *ptr;
  else
    return -1;
} 

/* Returns the block device sector that contains byte offset POS
   within INODE from our more complex structure
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector_new (struct inode *inode, off_t pos)
{
  ASSERT (inode != NULL);
  return get_sector(&inode->data, pos);
} 


/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
  id_count = 0;
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  printf("\nIn inode_create...\n");
  struct inode_disk *disk_inode = NULL;
  bool success = length == 0 ? true : false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  // printf("original length: %d\n", length);

  // printf ("Before calloc: %p\n", disk_inode);
  // printf("\tsizeof disk_inode = %d\n",sizeof disk_inode);
  // printf("\tsizeof *disk_inode = %d\n",sizeof *disk_inode);
  disk_inode = calloc (1, sizeof *disk_inode);
  // printf("\tinode sector = %d\n",sector);
  // printf ("After calloc: %p\n", disk_inode);
  if (disk_inode != NULL) {
      // printf("\tdisk_inode: %p\n", disk_inode);
      disk_inode->id = id_count++;
      size_t sectors = bytes_to_sectors (length);
      // printf("\tsectors: %d\n", sectors);
      // printf("\tlength: %d\n", length);
      disk_inode->length = length;
      printf("\tdisk_inode->id: %d, original length: %d\n", disk_inode->id, disk_inode->length);
      disk_inode->magic = INODE_MAGIC;
      // printf("sizeof (struct inode_indirect_block) = %d\n",sizeof (struct inode_indirect_block));
      disk_inode->ib_0 = calloc (1, sizeof (struct inode_indirect_block));
      // printf("\tcalloced disk_inode->ib_0\n");
      free_map_allocate(1, &disk_inode->ib0_sector);  //Allocate sector for index block 0
      // printf("\tib0_sector = %d\n",disk_inode->ib0_sector);
      disk_inode->ib_1 = calloc (1, sizeof (struct inode_indirect_block));
      // printf("\tcalloced disk_inode->ib_1\n");
      free_map_allocate(1, &disk_inode->ib1_sector);  //Allocate sector for index block 1
      // printf("\tib1_sector = %d\n",disk_inode->ib1_sector);
      if(disk_inode->ib_0 == NULL || disk_inode->ib_1 == NULL)
        return false;
      // free_map_allocate (1, get_ptr_for_sector(disk_inode, 0));
      int i, j;
      block_sector_t * sector_ptr;
      for (i = 0, j = 0; i < sectors; i++, j += BLOCK_SECTOR_SIZE) {
        int first_index = (i - 138) / 128;
        //printf("first_index = %d\n", first_index);
        if (i >= 138){
          //printf("evidently this is a large file\n");
          int second_index = (i - 138) % 128;
          struct inode_indirect_block *indirect = calloc (1, sizeof (struct inode_indirect_block));
          if(!second_index){
            free_map_allocate(1, &disk_inode->ib_1->blocks[first_index]);
          }
          else{
            //indirect = palloc_get_page(0);
            //printf("disk_inode->ib_1->blocks[first_index] = %d, first_index = %d, i = %d\n", disk_inode->ib_1->blocks[first_index], first_index, i);
            block_read(fs_device, disk_inode->ib_1->blocks[first_index], indirect);
          }
          free_map_allocate(1, &indirect->blocks[second_index]);
          static char zeros[BLOCK_SECTOR_SIZE];
          block_write(fs_device, indirect->blocks[second_index], zeros);
          block_write(fs_device, disk_inode->ib_1->blocks[first_index], indirect);
          block_write(fs_device, disk_inode->ib1_sector, disk_inode->ib_1);
          // int k;
          // for(k = 0; k < 128; k++){
          //   if(k%50 == 0)
          //     printf("\n");
          //   printf("%d, ",indirect->blocks[k]);
          // }
          // printf("\n");
          // if(indirect != NULL)
          //free(indirect);
          success = true;
        }
        else{
          sector_ptr = get_ptr_for_sector(disk_inode, j);
          if (free_map_allocate (1, sector_ptr) )
          {
              block_write(fs_device, disk_inode->ib0_sector, disk_inode->ib_0);
              // printf("i: %d, j: %d\n",i, j);
              // printf("sector number: %d\n", *get_ptr_for_sector(disk_inode, j));
              // block_write (fs_device, sector, disk_inode);
              static char zeros[BLOCK_SECTOR_SIZE];
              // printf("disk_inode: length = %d, sector for ib_0 = %d, sector for ib_1 = %d\nstart:\n",disk_inode->length, disk_inode->ib0_sector, disk_inode->ib1_sector);
              //int k;
              // printf("start printing arrays in inode_create:\n");
              // for (k = 0; k < 10; k++) {
              //   printf("%d, ", disk_inode->start[k]);
              // }
              // printf("\n");
              // for (k = 0; k < 128; k++){
              //   if(k % 50 == 0)
              //     printf("\n");
              //   printf("%d, ", disk_inode->ib_0->blocks[k]);
              // }
              // printf("\n");
              // printf("end printing arrays in inode_create.\n");
              // printf("\n");
              // printf("get_sector: %d", get_sector(disk_inode, j));
              // printf("Before block_write...\n");
              block_write (fs_device, *sector_ptr, zeros);
              // printf("\tblock wrote to sector: %d\n", *sector_ptr);
              success = true; 
          } 
          else
          {
            printf("returning false in create\n");
            success = false;
            break;
          }
        }
      }
      // PANIC("Out of inode_create!\n");
      // printf("\tdisk_inode: length = %d\n", disk_inode->length); //sector for ib_0 = %d, sector for ib_1 = %d\n",disk_inode->length, disk_inode->ib0_sector, disk_inode->ib1_sector);
      block_write (fs_device, sector, disk_inode);
      // printf("\tON DISK block wrote to sector: %d\n", sector);
      block_write(fs_device, disk_inode->ib0_sector, disk_inode->ib_0);
      block_write(fs_device, disk_inode->ib1_sector, disk_inode->ib_1);
      // printf("Before free...\n");
      //free(disk_inode->ib_0);
      //free(disk_inode->ib_1);
      // printf("length is %d\n",disk_inode->length);
      free (disk_inode);
      // printf("After free...\n"); 
    }
  //printf("success = %d\n", success);
  printf("Out of inode_create\n");
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  // printf("\nIn inode_open\n");
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          // printf("in inode_open above inode_reopen\n");
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL) {
    return NULL;
  }

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  block_read (fs_device, inode->sector, &inode->data);
  // printf("\tinode sector: %d, inode->data.length: %d, inode->id: %d\n", inode->sector, inode->data.length, inode->data.id);
  // printf("Out of inode_open\n");
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  // printf("\nIn inode_open\n");
  if (inode != NULL)
    inode->open_cnt++;
  // printf("Out of inode_open\n");
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk. (Does it?  Check code.)
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  // printf("\nIn inode_close\n");
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* ************* NOTE **************
     We were only writing the file_header to the disk, is that right
     or do we need to write every sector to the disk? Including the
     ib_0 and ib_1 sectors? Uncomment these lines to see what it does... */

  block_write(fs_device, inode->sector, &inode->data);
  // block_write(fs_device, inode->data.ib0_sector, inode->data.ib_0);
  // block_write(fs_device, inode->data.ib1_sector, inode->data.ib_1);
  // //printf("\tblock wrote to sector: %d, id: %d\n", inode->sector, inode->data.id);
  size_t sectors = bytes_to_sectors (inode->data.length);
  int i, j;
  // for (i = 0, j = 0; i < sectors; i++, j += BLOCK_SECTOR_SIZE) 
  // {
  //   printf("Writing sector: %d\n", get_sector(&inode->data, j));
  //   block_write (fs_device, get_sector(&inode->data, j), 1);
  // }

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      // block_write(fs_device, inode->sector, &inode->data);
      // printf("\tblock wrote to sector: %d, id: %d\n", inode->sector, inode->data.id);

      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          //free_map_release (inode->sector, 1);
          //free_map_release (inode->data.start[0],
                            //bytes_to_sectors (inode->data.length)); 
          for (i = 0, j = 0; i < sectors; i++, j += BLOCK_SECTOR_SIZE) 
          {
            // printf("Releasing sector: %d\n", get_sector(&inode->data, j));
            free_map_release (get_sector(&inode->data, j), 1);
          }
          free_map_release (inode->data.ib0_sector, 1);
          free_map_release (inode->data.ib1_sector, 1);
          free_map_release (inode->sector, 1);
        }
      free (inode); 
    }
    // printf("Out of inode_close\n");
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  printf("\nIn inode_read_at\n");
  printf("\tsize: %d, file: %d, length: %d\n", size, inode->data.id, inode->data.length);
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector_new (inode, offset); // Changed from old
      //printf("sector index: %d, offset = %d\n", sector_idx, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          block_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          block_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);
  printf("Out of inode_read_at\n");
  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
 printf("\nIn inode_write_at\n");
 printf("\tsize: %d, id: %d, offset: %d, file size: %d\n", size, inode->data.id, offset, inode->data.length);
//  int k;
//  printf("start printing arrays inode_write_at:\n");
//  for (k = 0; k < 10; k++) {
//     printf("%d, ", inode->data.start[k]);
//  }
//  printf("\n");
//  for (k = 0; k < 128; k++){
//     if(k % 50 == 0)
//       printf("\n");
//     printf("%d, ", inode->data.ib_0->blocks[k]);
//  }
//  printf("\n");
//  printf("end printing arrays inode_write_at.\n");
  //printf("in inode_write_at\n");
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;
  bool extended = false;

  if (inode->deny_write_cnt)
    return 0;
 
  if (offset + size > inode->data.length && ((offset + size) % BLOCK_SECTOR_SIZE) <= size){
    // if (offset != 0)
      // printf("(offset + size) mod BLOCK_SECTOR_SIZE) = %d\n", ((offset + size) % BLOCK_SECTOR_SIZE));
    int i = 0;
    for (i = offset; i < size + offset; i += BLOCK_SECTOR_SIZE) {
      block_sector_t * orig_ptr = get_ptr_for_sector(&inode->data, offset);
      free_map_allocate(1, orig_ptr);
    }
    block_write(fs_device, inode->sector, &inode->data);
    block_write(fs_device, inode->data.ib0_sector, inode->data.ib_0);
  }

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
     // printf("inode_write_at length: %d, magic: %d\n", inode->data.length, inode->data.magic);
      block_sector_t * sector_ptr = get_ptr_for_sector(&inode->data, offset);
        printf("\tsector: %d\n", *sector_ptr);
      if (offset + size > inode->data.length) {
        printf("offset + size: %d\n", offset + size);
        extended = true;
        //Allocate a new sector to write to, sector_ptr probably garbage
        // free_map_allocate(1, sector_ptr);
        inode->data.length += size > BLOCK_SECTOR_SIZE ? BLOCK_SECTOR_SIZE : size % BLOCK_SECTOR_SIZE;
        // block_write(fs_device, inode->sector, &inode->data);
        // block_write(fs_device, inode->data.ib0_sector, inode.data->ib_0);
        printf("\tsector after allocating: %d\n", *sector_ptr);
        int k;
              for (k = 0; k < 10; k++) {
                printf("%d, ", inode->data.start[k]);
              }
              printf("\n");
              for (k = 0; k < 128; k++){
                if(k % 50 == 0)
                  printf("\n");
                printf("%d, ", inode->data.ib_0->blocks[k]);
              }
      }
      block_sector_t sector_idx = *sector_ptr; // Changed from old
      // if ( (sector_idx > 16384) || ((offset / BLOCK_SECTOR_SIZE) >= 10 && sector_idx == 0) ) {// FIX THIS LATER, Maybe revisit zeroing things
      //   sector_ptr = get_ptr_for_sector (&inode->data, offset);
      //   free_map_allocate(1, sector_ptr);  // Allocate sector for a direct block in the first indirect block if it doesn't exist
      //   sector_idx = *sector_ptr;
      //   // printf("allocated in inode_write_at\n");
      // }
      // printf("\tIn while loop, sector_idx: %d, size: %d\n", sector_idx, size);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;


      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          block_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            block_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          block_write (fs_device, sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);
  printf("Out of inode_write_at, bytes_written is :%d\n", bytes_written);
  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}

