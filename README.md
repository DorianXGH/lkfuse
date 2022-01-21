# lkfuse
LKFS Fuse driver repository

## The filesystem

LKFS is a filesystem described in [this file](LKFS.md).

It revolves around linked lists where the links are maintained in a table at the beginning of the file system. 
This table also serves as an allocation table for blocks.

There's no separation of inodes and regular blocks, everything is a block.

## The utilities

The lkfs binary can create the filesystem from any file.

The program usage is ```$ ./lkfs init <filename> <size>```.

Size format is ```<n>(K/M/G/T)```, it must be a multiple of 2MB, so for a 4GB filesystem, the size is written ```4G```.
