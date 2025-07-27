# Makefile for Multiple Planetary Alignment Finder

# The C compiler to use.
CC = gcc

# The name of the final executable.
TARGET = multi_alignment_finder

# All C source files used in the project.
SRCS = multi_alignment_finder.c

# CFLAGS: Flags passed to the C compiler.
CFLAGS = -Wall -O2 -std=c99

# LDFLAGS: Flags passed to the linker.
# We need to link the Math library.
LDFLAGS = -lm

# --- Build Rules ---

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(SRCS) -o $(TARGET) $(CFLAGS) $(LDFLAGS)

clean:
	rm -f $(TARGET)
