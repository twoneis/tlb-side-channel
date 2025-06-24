obj-m += lkm.o

CC := gcc

SOURCES := $(filter-out lkm.c, $(wildcard *.c))
TARGETS := $(SOURCES:.c=.o)

CFLAGS += -g
CFLAGS += -O2
CFLAGS += -Wall
CFLAGS += -Wextra
CFLAGS += -D_FILE_OFFSET_BITS=64

build: leak.o threshold.o dpm_leak.o

run: leak.o
	./leak.o

threshold: threshold.o
	./threshold.o

dpm: dpm_leak.o
	./dpm_leak.o

clean:
	rm -f *.o

module:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) modules

clean-module:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) clean

insert: module
	insmod lkm.ko
	chmod 666 /dev/lkm

remove: clean-module
	rmmod lkm

%.o: %.c
	$(CC) $< $(CFLAGS) -o $@

