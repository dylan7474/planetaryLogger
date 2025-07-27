# Makefile for SDL Solar System Visualizer

# The C compiler to use.
CC = gcc

# The name of the final executable.
TARGET = sdl_visualizer

# All C source files used in the project.
SRCS = visualizer.c

# CFLAGS: Flags passed to the C compiler.
# We get the necessary flags from the sdl2-config tool.
CFLAGS = -Wall -O2 -std=c99 `sdl2-config --cflags`

# LDFLAGS: Flags passed to the linker.
# We get the necessary library flags from the sdl2-config tool
# and add the ones for SDL_ttf and the math library.
LDFLAGS = `sdl2-config --libs` -lSDL2_ttf -lm

# --- Build Rules ---

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(SRCS) -o $(TARGET) $(CFLAGS) $(LDFLAGS)

clean:
	rm -f $(TARGET)
