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
  printf("pos: %d, length: %d\n", pos, inode_disk->length);
  if (pos <= inode_disk->length)
  {
    //printf("pos / BLOCK_SECTOR_SIZE %d\n", pos / BLOCK_SECTOR_SIZE);
    if (pos / BLOCK_SECTOR_SIZE < 10)
    {
      printf("direct block\n");
      //printf("Small file\n");
      /* Our pos is within the first 10 direct blocks */
      printf("sector pos: %d\n", pos/BLOCK_SECTOR_SIZE);
      return &(inode_disk->start[pos / BLOCK_SECTOR_SIZE]);
    }
    else if (pos /BLOCK_SECTOR_SIZE < 138)
    {
      printf("indirect block\n");
      /* Our pos is within the first level of indirection */
      // printf("ib0_sector: %\n");
      block_read(fs_device, inode_disk->ib0_sector, inode_disk->ib_0);
      //printf("After getting to indirect block\n");
      int index = (pos / BLOCK_SECTOR_SIZE) - 10;
      printf("Index in array is %d, pos is: %d, BLOCK_SECTOR_SIZE is: %d\n", index, pos, BLOCK_SECTOR_SIZE);
      //printf("Accessing index in array: %d\n", blah->blocks[1]);
      return &((inode_disk->ib_0)->blocks[index]); 
    }
    else if (pos / BLOCK_SECTOR_SIZE < 16522 )
    {
      printf("doubly indirect block\n");
      /* Our pos is within the second level of indirection... uhh... */
      int first_index = ((pos / BLOCK_SECTOR_SIZE) - 138) / 128;
      int second_index = ((pos / BLOCK_SECTOR_SIZE) - 138) % 128;
      return &(inode_disk->ib_1->ibs[first_index]->blocks[second_index]); // TODO
    }
    printf("inside first if\n");
    return -1; // Something went wrong!
  }
  else {
    printf("outside first if\n");
    return -1;
  }
} 

static block_sector_t
get_sector (const struct inode_disk *inode_disk, off_t pos)
{
  block_sector_t * ptr = get_ptr_for_sector(inode_disk, pos);
  // printf("index pointer: %p, index: %d\n", ptr, *ptr);
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
byte_to_sector_new (const struct inode *inode, off_t pos)
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
  //printf("in inode_create\n");
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  //printf("original length: %d\n", length);

  disk_inode = calloc (1, sizeof disk_inode);
  if (disk_inode != NULL)
    {
      disk_inode->id = id_count++;
      size_t sectors = bytes_to_sectors (length);
      printf("sectors: %d, length: %d\n", sectors, length);
      // PANIC("BLAH");
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      disk_inode->ib_0 = calloc (1, sizeof (struct inode_indirect_block));
      free_map_allocate(1, &disk_inode->ib0_sector);  //Allocate sector for index block 0
      disk_inode->ib_1 = calloc (1, sizeof (struct inode_doubly_indirect_block));
      free_map_allocate(1, &disk_inode->ib1_sector);  //Allocate sector for index block 1
      if(disk_inode->ib_0 == NULL || disk_inode->ib_1 == NULL)
        return false;
      // free_map_allocate (1, get_ptr_for_sector(disk_inode, 0));
      block_write (fs_device, sector, disk_inode);
      int i, j;
      for (i = 0, j = 0; i < sectors; i++, j += BLOCK_SECTOR_SIZE) {
        if (free_map_allocate (1, get_ptr_for_sector(disk_inode, j))) 
        {
            // block_write (fs_device, sector, disk_inode);
            static char zeros[BLOCK_SECTOR_SIZE];
            block_write (fs_device, get_sector(disk_inode, j), zeros);
            success = true; 
        } 
        else
        {
          success = false;
          break;
        }
      }
     // printf("out of inode_create, length is: %u, success is: %d\n", disk_inode->length, success);
      free (disk_inode);
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  block_read (fs_device, inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
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
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
          free_map_release (inode->data.start[0],
                            bytes_to_sectors (inode->data.length)); // Maybe change this?
        }

      free (inode); 
    }
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
  printf("reading, size: %d, file: %d\n", size, inode->data.id);
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector_new (inode, offset); // Changed from old
      // printf("sector index: %d\n", sector_idx);
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
  printf("writing\n");
  //printf("in inode_write_at\n");
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
     // printf("inode_write_at length: %d, magic: %d\n", inode->data.length, inode->data.magic);
      block_sector_t sector_idx = byte_to_sector_new (inode, offset); // Changed from old
      // printf("sector index: %d\n", sector_idx);
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

