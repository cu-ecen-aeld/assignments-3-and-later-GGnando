CC := gcc
CFLAGS := -g -Wall

# Support for cross-compilation
ifdef CROSS_COMPILE
CC := $(CROSS_COMPILE)gcc
endif

default: writer

writer: writer.c
	$(CC) $(CFLAGS) -o writer writer.c

clean:
	rm -f writer *.o