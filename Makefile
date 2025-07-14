#
# A simple makefile for managing build of project composed of C source files.
#

# Default compiler is clang. There is a "build_gcc" rule you can use for the GCC compiler
CC=clang
CFLAGS= -fcolor-diagnostics -fansi-escape-codes -g -std=c99 -Wall -Wextra -pedantic

# The LDFLAGS variable sets flags for linker
#  -lm   says to link in libm (the math library)
LDFLAGS = -lm

SRC_DIR = src
OBJ_DIR = generated_bins

MAIN_FILE_SRC=assemble_and_run.c
MAIN_FILE_OBJ=$(OBJ_DIR)/assembler_and_run.o

SAMPLE_ARGS=tests/programs/arith.ryasm tests/programs/arith.ryasm.ryc

TARGET=generated_bins/ryvm

#Get List of source files. Make cannot search recursively, so we will add subfolders here
SRCS=$(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/error/*.c) $(wildcard $(SRC_DIR)/logger/*.c) $(wildcard $(SRC_DIR)/memory/*.c)

#use pattern matching and patsubst function to replace the src/ in each SRCS file with generated_bins/
#these are the names of our object files
OBJS=$(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))





#our current "compile and link" rule
all: $(TARGET)


# target specific values can be set like so
build_gcc: CC=gcc 
build_gcc: CFLAGS= -Wall -Wextra -pedantic -std=c99 -g 
build_gcc: LDFLAGS = -lm
build_gcc: all  # 


# run the executable with a list of arguments
sample: $(TARGET)
	$(TARGET) $(SAMPLE_ARGS)




#link all object files to single target
$(TARGET): $(OBJS) $(MAIN_FILE_OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(MAIN_FILE_OBJ)



# compile each source file to its appropriate .o file ONLY IF the source file was modified
$(OBJ_DIR)/%.o : $(SRC_DIR)/%.c 
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

#separate rule needed since main file is not in src directory
$(MAIN_FILE_OBJ): $(MAIN_FILE_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

# Phony means not a "real" target, it doesn't build anything
# The phony target "clean" is used to remove all compiled object files.
# 'core' is the name of the file outputted in some cases when you get a
# crash (SEGFAULT) with a "core dump"; it can contain more information about
# the crash.
.PHONY: clean

clean:
	$(RM) $(TARGET) $(OBJS) $(MAIN_FILE_OBJ)
