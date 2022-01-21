#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include "lkfs.h"
 
FILE *devfile = NULL;
uint64_t blocksize = 0x1000; // 4K blocks

uint64_t capacity_parse(char * cap)
{
    int l = strlen(cap);
    char * capw = (char *) malloc(l*sizeof(char));
    if(capw != NULL)
    {
      strcpy(capw,cap);
      char unit = capw[l-1];
      capw[l-1] = 0;
      uint64_t sz = strtoull(capw,0,0);
      free(capw);
      switch(unit|0b100000) {
        case 'k':
          sz <<= 10;
          break;
        case 'm':
          sz <<= 20;
          break;
        case 'g':
          sz <<= 30;
          break;
        case 't':
          sz <<= 40;
          break;
        default:
          return -1;
          break;
      }
      return sz;
    }
    else 
    {
      return -1;
    }
}

void gen_GUID(uint8_t * GUID)
{
  // xx xx xx xx - xx xx - Mx xx - Nx xx -xxxxxxxxxxxx
  for(int i = 0; i < 16; i++)
  {
    GUID[i] = rand()&0xff;
  }
  GUID[6] = 0x40 | (GUID[6] & 0xf);
  GUID[8] = 0x80 | (GUID[8] & 0x3f);  
}

uint64_t block_link_table_size(uint64_t sz, uint64_t blk_sz)
{
  return sizeof(uint64_t)*sz/(blk_sz*blk_sz);
}

void set_block_link(uint64_t block, uint64_t link)
{
  fseek(devfile,blocksize+sizeof(uint64_t)*block,SEEK_SET);
  fwrite(&link,sizeof(uint64_t),1,devfile);
}

void init_block_link_table(uint64_t sz)
{
  uint64_t tbl_sz = block_link_table_size(sz,blocksize);
  printf("tblsz : %llx\n",tbl_sz);
  set_block_link(0,0);
  for(uint64_t i = 1; i < tbl_sz+1; i++)
  {
    set_block_link(i,i+1);
  }
  set_block_link(tbl_sz,0);
  for(uint64_t k = tbl_sz + 1; k < (sz/blocksize); k++)
  {
    set_block_link(k,-1);
  }
}
 
int main(int argc, char **argv)
{
  srand(time(NULL)); 
  if(argc < 4)
    return -1;
  int i;
  if(strcmp(argv[1],"init") == 0)
  {
    uint64_t sz = capacity_parse(argv[3]);
    if(sz == -1 || sz == 0)
      return -2;
    if((sz%(2<<20))==0)
    {
      devfile = fopen(argv[2],"wb+");
      struct SUPERBLOCK * superb = malloc(sizeof(struct SUPERBLOCK)); // from now on, no check on malloc
      superb->MAGIC[0] = 'L';
      superb->MAGIC[1] = 'K';
      superb->MAGIC[2] = 'F';
      superb->MAGIC[3] = 'S';
      superb->OS = OS_FUSE;
      superb->BLK_SZ = 0; // 4K blocks
      superb->VERSION = 0;
      superb->BLK_LK_TBL = 1;
      superb->RT = -1;
      gen_GUID((uint8_t *)&(superb->GUID));
      fwrite(superb,sizeof(struct SUPERBLOCK),1,devfile);
      init_block_link_table(sz);
      fseek(devfile,sz-1,SEEK_SET);
      char a[1]="\0";
      fwrite(a,sizeof(char),1,devfile);
      free(superb);
      fclose(devfile);
    }
    else
    {
      return -3;
    }
  }
}