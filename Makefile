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


SAMPLE_VM_ARGS=tests/programs/arith.ryasm.ryc
SAMPLE_AS_ARGS=tests/programs/arith.ryasm $(SAMPLE_VM_ARGS)

AS_TARGET=$(OBJ_DIR)/ryasm
VM_TARGET=$(OBJ_DIR)/ryvm

# WATCH OUT FOR TRAILING SPACES. They are counted as part of the string even if you did not want them
AS_SRC_DIR=$(SRC_DIR)/assembler
VM_SRC_DIR=$(SRC_DIR)/vm

AS_OBJ_DIR=$(OBJ_DIR)/assembler
VM_OBJ_DIR=$(OBJ_DIR)/vm


#These hold all files that are used in both the VM target and the assembler target.
SHARED_SRCS=$(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/memory/*.c)
SHARED_OBJS=$(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SHARED_SRCS))


#Get List of source files. Make cannot search recursively, so we will add subfolders here
AS_SRCS=$(wildcard $(AS_SRC_DIR)/*.c)


# Used to print variable from Makefile for basic debugging 
$(info $(AS_SRCS))

#use notdir to remove the directory part of the AS_SRCS files and replace with our object directory
#AS_OBJS=$(addprefix $(AS_OBJ_DIR)/,$(notdir $(AS_SRCS)))


# Watch out, DO NOT PUT SPACES between COMMAS in function arguments, the spaces will be included in the 
# argument itself and will mess up the string replacement.
AS_OBJS=$(patsubst $(AS_SRC_DIR)/%.c,$(AS_OBJ_DIR)/%.o,$(AS_SRCS))

# Used to print variable from Makefile for basic debugging 
$(info $(AS_OBJS))


VM_SRCS=$(wildcard $(VM_SRC_DIR)/*.c) 
VM_OBJS=$(patsubst $(VM_SRC_DIR)/%.c,$(VM_OBJ_DIR)/%.o,$(VM_SRCS))


#our current "compile and link" rule
all: build_asm build_vm


run_sample: run_asm_sample run_vm_sample


# run the assembler with a sample set of arguments
run_asm_sample: $(AS_TARGET)
	$(AS_TARGET) $(SAMPLE_AS_ARGS)


# run the vm with a sample set of arguments
run_vm_sample: $(VM_TARGET)
	$(VM_TARGET) $(SAMPLE_VM_ARGS)




build_asm: $(AS_TARGET)

build_vm: $(VM_TARGET)


# target specific values can be set like so
build_gcc: CC=gcc 
build_gcc: CFLAGS= -Wall -Wextra -pedantic -std=c99 -g 
build_gcc: LDFLAGS = -lm
build_gcc: all  


#link all object files to single target
$(AS_TARGET): $(AS_OBJS) $(SHARED_OBJS)
	$(CC) $(CFLAGS) -o $@ $(AS_OBJS) $(SHARED_OBJS)


$(VM_TARGET): $(VM_OBJS) $(SHARED_OBJS)
	$(CC) $(CFLAGS) -o $@ $(VM_OBJS) $(SHARED_OBJS)




#dont use this since this implies that all object files are dependent on ALL source files,
#not just the source file that the object is compiled from.
#$(AS_OBJS): $(AS_SRCS)
#	mkdir -p $(@D)
#	$(CC) $(CFLAGS) -c $< -o $@
	

#compile source files to object files only if they were modified

#Use pattern matching so that each object file only depends on the source file that it was compiled from.
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@



# Phony means not a "real" target, it doesn't build anything
# The phony target "clean" is used to remove all compiled object files.
# 'core' is the name of the file outputted in some cases when you get a
# crash (SEGFAULT) with a "core dump"; it can contain more information about
# the crash.
.PHONY: clean

clean:
	$(RM) -r $(OBJ_DIR)/*

