# Source files
SRCS := $(wildcard *.c) $(wildcard engine/*.c) $(wildcard utils/*.c) $(wildcard moonnuklear/*.c)
# Header files
HEADERS := $(wildcard *.h) $(wildcard engine/*.h) $(wildcard utils/*.h)

# Object files directory
OBJDIR := build
# Object files
OBJS := $(SRCS:%.c=$(OBJDIR)/%.o)

# Compiler and flags
CC := gcc
CFLAGS := -D_GLFW_WAYLAND -DUSE_WAYLAND=ON -g -fpermissive -llua -lraylib -lGL -lm -lpthread -ldl -lrt -lc

# Program name
PROGRAM := program

# Clang format targets
CLANG_FORMAT_TARGETS := $(SRCS) $(HEADERS)

.PHONY: all format clean

# Default target
all: build

# Format the source code
format:
	clang-format -i --style=LLVM --style="{BasedOnStyle: llvm, IndentWidth: 4}" $(CLANG_FORMAT_TARGETS)

# Build the program
build: $(PROGRAM)

# Link object files to create the final executable
$(PROGRAM): $(OBJS)
	$(CC) $(OBJS) -o $(PROGRAM) $(CFLAGS)

# Compile source files into object files
$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -c $< -o $@ $(CFLAGS)

# Clean up
clean:
	rm -rf $(OBJDIR) $(PROGRAM)

# Run the program
run: #$(PROGRAM)
	./$(PROGRAM)

# Build and run the program
build_run: format build run
