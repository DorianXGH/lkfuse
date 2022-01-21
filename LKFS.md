LKFS
===

partition start and end 2M aligned, everything in little endian

Block[0] : superblock
{
MAGIC, 4B, "LKFS"
OS 2B
VERSION 1B
BLK_SZ 1B, block size = 4K * (1 << BLK_SZ)
BLK_LK_TBL 8B, block number of the start of the block linking table
BLK_RT 8B, block number of the first descriptor of the root directory, -1 means empty root directory.
GUID 16B, GUID of the filesystem
}

Block linking table : huge array of 8B "next block" pointers, the next block being BLT[block]
0 means there's no next block,
-1 means the block in unallocated

Descriptor Block
{
    TYPE, 1B, 0 : file, 1 : directory, 2 : hard link, 3 : symbolic link
    PERMS, 3B, PERMS[0] owner, PERMS[1] group, PERMS[2] everyone
    OS_SPEC, 4B
    NAME_BLK, 8B, block number of the start of the name's block list, 0 if full name in descriptor

    UID, 8B
    GID, 8B
    GUID, 16B

    CONTENT_BLK, 8B, block number of the start of the content's block list, in case of hard link, contains the block ID of the corresponding descriptor block, in case of symlink, the content is the file path. For directories, -1 means empty.

    CTIME 8B, creation time in posix format
    MTIME 8B, last modification time
    ATIME 8B, last access time

    SIZE 8B, content size in bytes for non-compound types
    
    HARD_LNK_CNT 1B, hard link count

    NAME NB, the name of the descriptor
}