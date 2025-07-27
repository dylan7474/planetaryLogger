# Makefile for Planetary Distance Calculator on Raspberry Pi

# The C compiler to use.
CC = gcc

# The name of the final executable.
TARGET = planetary_logger

# All C source files used in the project.
SRCS = main.c

# CFLAGS: Flags passed to the C compiler.
# -Wall: Enable all warnings
# -O2: Optimization level 2
# -std=c99: Use the C99 standard
CFLAGS = -Wall -O2 -std=c99

# LDFLAGS: Flags passed to the linker.
# We need to link the cURL, Jansson, and Math libraries.
LDFLAGS = -lcurl -ljansson -lm

# --- Build Rules ---

# The default target.
all: $(TARGET)

# Rule to link the final executable.
$(TARGET): $(SRCS)
	$(CC) $(SRCS) -o $(TARGET) $(CFLAGS) $(LDFLAGS)

# Rule to clean up the build directory.
clean:
	rm -f $(TARGET)
