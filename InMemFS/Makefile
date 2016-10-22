CC=gcc
CFLAGS=-g -Wall

all: ramdisk

ramdisk:	ramdisk.c
	$(CC) $(CFLAGS) ramdisk.c `pkg-config fuse --cflags --libs` -o $@

clean:
	\rm -f ramdisk
