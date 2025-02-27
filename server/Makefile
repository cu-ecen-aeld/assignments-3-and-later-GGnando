CC := gcc
CFLAGS = -Wall -Wextra -Werror -g
TARGET = aesdsocket
OUTPUT_DIR = .
SOURCE=$(OUTPUT_DIR)/aesdsocket.c
OUTPUT = $(OUTPUT_DIR)/$(TARGET)

# Support for cross-compilation
ifdef CROSS_COMPILE
CC := $(CROSS_COMPILE)gcc
endif

default: all
all: $(TARGET)

$(OUTPUT): $(SOURCE)
	$(CC) $(CFLAGS) -o $(OUTPUT) $(SOURCE)

clean:
	rm -f $(OUTPUT)
