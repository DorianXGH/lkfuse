CC := gcc
CFLAGS_FUSE := -I/usr/include/fuse -lfuse -D_FILE_OFFSET_BITS=64

all: lkfs lkfuse

lkfs: lkfs.c lkfs.h
	gcc $< -o $@

lkfuse: lkfuse.c lkfs.h
	gcc $(CFLAGS_FUSE) $< -o $@

clean:
	rm lkfs
	rm lkfuse
	rm fsfile.bin

fsfile.bin: lkfs
	./lkfs init ./fsfile.bin 2M

mount: lkfuse fsfile.bin
	./lkfuse -d -f -s fsfile.bin ./test