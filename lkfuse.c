#define FUSE_USE_VERSION 29
#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <libgen.h>
#include "lkfs.h"
 
char *devfilepath = NULL;
FILE* devfile;
uint64_t blocksize;
uint64_t fssize;
struct SUPERBLOCK sprblk;

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

void rewrite_superblock()
{
  fseek(devfile,0,SEEK_SET);
  fwrite(&sprblk,sizeof(struct SUPERBLOCK),1,devfile);
}

bool user_has_right(struct DESCRIPTOR * desc, uint64_t right, uint64_t uid, uint64_t gid)
{
  uint8_t categ = (desc->UID == uid) ? 0 : ((desc->GID == gid) ? 1 : 2);
  return desc->PERMS[categ] & right;
}

void set_block_link(uint64_t block, uint64_t link)
{
  fseek(devfile,blocksize+sizeof(uint64_t)*block,SEEK_SET);
  fwrite(&link,sizeof(uint64_t),1,devfile);
}

uint64_t get_block_link(uint64_t block)
{
  fseek(devfile,blocksize+sizeof(uint64_t)*block,SEEK_SET);
  uint64_t link;
  fread(&link,sizeof(uint64_t),1,devfile);
  return link;
}

uint64_t find_free_block()
{
  for(uint64_t blk = 1; blk < fssize/blocksize; blk++){
    if(get_block_link(blk)==-1)
      return blk;
  }
  return 0;
}

void* get_block(uint64_t block)
{
  void * block_data = malloc(blocksize*sizeof(uint8_t));
  fseek(devfile,block*blocksize,SEEK_SET);
  fread(block_data,blocksize*sizeof(uint8_t),1,devfile);
  return block_data;
}

void set_block(uint64_t block, void* data)
{
  fseek(devfile,block*blocksize,SEEK_SET);
  fwrite(data,blocksize*sizeof(uint8_t),1,devfile);
}

bool is_descriptor_name_eq(struct DESCRIPTOR * desc, char * dir)
{
  return strcmp(desc->NAME,dir) == 0;
}

uint64_t get_block_from_path(char* path)
{
  if(strcmp(path,"/")==0)
  {
    return 0;
  }
  char* internal;
  uint64_t current_block = sprblk.RT;
  uint64_t found_block;
  uint64_t current_type = TYPE_DIR;

  struct DESCRIPTOR current = {
    TYPE_DIR,
    {07,05,05},
    0,
    0,
    getuid(),
    getgid(),
    current_block,
    0,
    0,
    0,
    0,
    2,
    {'/'}
  }; 

  printf("Root is %llx\n",current_block);
  if(current_block == -1)
  {
    return -ENOENT;
  }
  for(char* dir = strtok_r(path,"/",&internal); dir != NULL; dir = strtok_r(NULL,"/",&internal))
  {
    printf("Searching for %s\n",dir);
    if(current_type == TYPE_FILE)
    {
      printf("ENOTDIR\n");
      return -ENOTDIR;
    }
    if(!(user_has_right(&current,PERM_EX,getuid(),getgid())))
    {
      return -EACCES;
    }
    uint64_t blk = current_block;
    while(blk)
    {
      printf("Looking blk %llx\n",blk);
      void * blk_data = get_block(blk);
      bool eq = is_descriptor_name_eq((struct DESCRIPTOR *)blk_data,dir);
      free(blk_data);
      if (eq)
        break;
      blk = get_block_link(blk);
    }
    if (blk)
    {
      found_block = blk;
      struct DESCRIPTOR * desc = (struct DESCRIPTOR *)get_block(found_block);
      current_block = desc->CONTENT_BLK;
      current_type = desc->TYPE;
      current = *desc;
      free(desc);
    }
    else
    {
      return -ENOENT;
    }
  }
  return found_block;
}

struct DESCRIPTOR * get_descriptor_from_block(uint64_t block)
{
  if(block==0)
  {
    struct DESCRIPTOR * rtdesc = (struct DESCRIPTOR *) malloc(sizeof(struct DESCRIPTOR));
    rtdesc->CONTENT_BLK = sprblk.RT;
    rtdesc->TYPE = TYPE_DIR;
    return rtdesc;
  }
  if((int64_t)block < 0)
    return block;
  struct DESCRIPTOR * desc = (struct DESCRIPTOR *)get_block(block);
  return desc;
}

struct DESCRIPTOR * get_descriptor_from_path(char* path)
{
  uint64_t blk = get_block_from_path(path);
  if((int64_t)blk < 0)
    return blk;
  return get_descriptor_from_block(blk);
}

uint16_t lkfs_mode_to_unix(struct DESCRIPTOR * desc)
{
  uint64_t type;
  switch(desc->TYPE)
  {
    case TYPE_FILE:
      type = __S_IFREG;
      break;
    case TYPE_DIR:
      type = __S_IFDIR;
      break;
    case TYPE_HRD_LNK:
    case TYPE_SYM_LNK:
      type = __S_IFLNK;
      break;
    default:
      type = -1;
      break;
  }
  type |= desc->PERMS[0]<<6 | desc->PERMS[1]<<3 | desc->PERMS[2];
  return (uint16_t)(type&0xffff);
}

int lkfs_getattr(const char *path, struct stat *st)
{
  if (!strcmp(path, "/")) {
    printf("Getting root attr\n");
    // it's the root directory (just an example, you probably have more directories)
    st->st_mode = __S_IFDIR | 0755; // access rights and directory type
    st->st_nlink = 2;             // number of hard links, for directories this is at least 2
    st->st_size = 4096;           // file size
    st->st_uid = getuid();
    st->st_gid = getgid();
  } else {
    printf("Getting attrs\n");
    struct DESCRIPTOR * desc = get_descriptor_from_path(path);
    printf("Desc is %llx\n",desc);
    if ((int64_t)desc < 0)
      return desc;
    printf("No errs\n",desc);
    st->st_mode = lkfs_mode_to_unix(desc); // access rights and regular file type
    st->st_nlink = desc->HARD_LNK_CNT;             // number of hard links
    st->st_size = (desc->TYPE==TYPE_FILE) ? desc->SIZE : 0x1000;           // file size
    st->st_uid = desc->UID;
    st->st_gid = desc->GID;
    st->st_atime = desc->ATIME;
    st->st_ctime = desc->CTIME;
    st->st_mtime = desc->MTIME;
  }
  return 0;
}

int lkfs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
  filler(buffer, ".", NULL, 0);       // current directory reference
  filler(buffer, "..", NULL, 0);      // parent directory reference
  struct DESCRIPTOR * desc = get_descriptor_from_path(path);
  if((int64_t)desc < 0)
    return desc;
  if(desc->CONTENT_BLK != -1)
  {
    printf("The directory isn't empty !\n");
    uint64_t blk = desc->CONTENT_BLK;
    while(blk != 0)
    {
      struct DESCRIPTOR * blk_dsc = (struct DESCRIPTOR *)get_block(blk);
      printf("Found %s\n",blk_dsc->NAME);
      filler(buffer, blk_dsc->NAME, NULL, 0); // any filename at path in your image
      free(blk_dsc);
      blk = get_block_link(blk);
    }
  }
  return 0;
}

int lkfs_mkdir(const char *path, mode_t mode)
{
  char * patha = strdup(path);
  char * pathb = strdup(path);

  printf("Path %s \n",path);

  char * parent = dirname(patha);
  char * filename = basename(pathb);

  printf("From parent dir %s , creating %s\n", parent, filename);
  if ((int64_t)get_block_from_path(path) >= 0) // exists
    return -EEXIST;
  uint64_t blk = get_block_from_path(parent);
  if((int64_t)blk < 0)
    return blk;
  struct DESCRIPTOR * desc = get_descriptor_from_block(blk);
  if(desc->TYPE != TYPE_DIR)
    return -ENOTDIR;
  if(!user_has_right(desc,__S_IWRITE>>6,getuid(),getgid()))
    return -EACCES;
  
  printf("Creating directory\n");
  struct DESCRIPTOR * newdir = (struct DESCRIPTOR *) malloc(blocksize*sizeof(uint8_t));
  newdir->TYPE = TYPE_DIR;
  newdir->UID = getuid();
  newdir->GID = getgid();
  newdir->CTIME = time(NULL);
  newdir->MTIME = newdir->CTIME;
  newdir->ATIME = newdir->CTIME;
  newdir->OS_SPEC = 0;
  newdir->NAME_BLK = 0;
  newdir->HARD_LNK_CNT = 1;
  newdir->CONTENT_BLK = -1;
  newdir->PERMS[0] = (mode & 0700)>>6;
  newdir->PERMS[1] = (mode & 0070)>>3;
  newdir->PERMS[2] = (mode & 0007);
  strncpy(newdir->NAME,filename,blocksize-sizeof(struct DESCRIPTOR)-1);
  newdir->NAME[blocksize-sizeof(struct DESCRIPTOR)-1] = 0;
  gen_GUID(newdir->GUID);
  //todo verify if directory already exists
  uint64_t newb = find_free_block();
  if(newb)
  {
    printf("Writing block\n");
    set_block(newb,newdir);
    uint64_t oldb = desc->CONTENT_BLK;
    desc->CONTENT_BLK = newb;
    if(oldb == -1)
      set_block_link(newb,0);
    else
      set_block_link(newb,oldb);
    if(blk != 0)
      set_block(blk,desc);
    else
      {
        sprblk.RT = newb;
        rewrite_superblock();
      }
    fflush(devfile);
    return 0;
  }
  return -ENOSPC;
}

int lkfs_chown (const char * path, uid_t uid, gid_t gid, struct fuse_file_info *fi)
{
  uint64_t blk = get_block_from_path(path);
  if((int64_t)blk < 0)
    return blk;
  if(blk != 0)
  {
    struct DESCRIPTOR * desc = get_descriptor_from_block(blk);
    desc->GID = (gid != -1) ? gid : desc->GID;
    desc->UID = (uid != -1) ? uid : desc->UID;
    set_block(blk,desc);
  } else
  {
    return -EPERM;
  }
  return 0;
}

int lkfs_chmod (const char * path, mode_t mode, struct fuse_file_info *fi)
{
  uint64_t blk = get_block_from_path(path);
  if((int64_t)blk < 0)
    return blk;
  if(blk != 0)
  {
    struct DESCRIPTOR * desc = get_descriptor_from_block(blk);
    desc->PERMS[0] = (mode & 0700)>>6;
    desc->PERMS[1] = (mode & 0070)>>3;
    desc->PERMS[2] = (mode & 0007);
    set_block(blk,desc);
  } else
  {
    return -EPERM;
  }
  return 0;
}


static struct fuse_operations myfs_ops = {
  .getattr = lkfs_getattr,
  .readdir = lkfs_readdir,
  .mkdir = lkfs_mkdir,
  .chown = lkfs_chown,
  .chmod = lkfs_chmod
};
 
int main(int argc, char **argv)
{
  int i;
 
  // get the device or image filename from arguments
  for (i = 1; i < argc && argv[i][0] == '-'; i++);
  if (i < argc) {
    devfilepath = realpath(argv[i], NULL);
    devfile = fopen(devfilepath,"rb+");
    fread(&sprblk,sizeof(sprblk),1,devfile);
    blocksize = 0x1000 << sprblk.BLK_SZ;
    fseek(devfile, 0L, SEEK_END);
    fssize = ftell(devfile);
    memcpy(&argv[i], &argv[i+1], (argc-i) * sizeof(argv[0]));
    argc--;
  }
  // leave the rest to FUSE
  return fuse_main(argc, argv, &myfs_ops, NULL);
}