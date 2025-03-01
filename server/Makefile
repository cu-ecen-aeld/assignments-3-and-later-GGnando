# Support for cross-compilation
CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Wextra -Werror -g
TARGET ?= aesdsocket
OUTPUT_DIR = .
SOURCE=$(OUTPUT_DIR)/aesdsocket.c
OUTPUT = $(OUTPUT_DIR)/$(TARGET)
HEADER =  queue.h
LDFLAGS ?= -lpthread -lrt

default: all
all: $(TARGET)

$(OUTPUT): $(SOURCE)
	$(CC) $(CFLAGS) $(HEADER) -o $(OUTPUT) $(SOURCE) $(LDFLAGS)

clean:
	rm -f $(OUTPUT)
