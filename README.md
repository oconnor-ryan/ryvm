# RYVM
A register-based process virtual machine and assembler. It can compile human-readable RYVM Assembly code into RYVM bytecode that can be run by the virtual machine.

> Note that this project is still in development and the instruction set is prone to change. 
> Don't use this project in a production environment. 

## TODO List
- Add support for dynamic memory allocations.
- Define a default calling convention for subroutines and syscalls.
- Fix issues where the bytewidth of a register is sometimes not respected and the full 64-bit register is processed.
- Fully implement the FPFX and FXFP instructions.
- Allow two or more compiled RYVM bytecode files to be linked into one RYVM executable.
- Allow programmers to configure a RYVM executable a "debug mode", where the VM can perform
  memory bounds-checking and print debug information when encountering an error that would normally
  crash the program (such as attempting to access out-of-bounds memory or executing an invalid opcode).
- Allow programmers to compile RYVM executable files into native assembly or machine code and store it
  in the appropriate executable format (ELF for Linux, MachO for MacOS, PE for Windows).

## Building the Project
Currently, I have a single build_vm.sh file that only runs on Unix-like systems (Linux or MacOS).
Here's a few examples to use when building the script:

`./build_vm.sh clang assemble_and_run.c # build the VM using Clang compiler and include the "assemble_and_run.c" file that contains the main() function to run a single RYVM executable.`

`./build_vm.sh gcc assemble_and_run.c # build the VM using GCC compiler and include the "assemble_and_run.c" file that contains the main() function to run a single RYVM executable.`

`./build_vm.sh clang tests/general_tests.c # build the VM using Clang compiler and include the "tests/general_tests.c" file that contains the main() function to run the test files in tests/programs.`

`./build_vm.sh gcc tests/general_tests.c # build the VM using GCC compiler and include the "tests/general_tests.c" file that contains the main() function to run the test files in tests/programs.`

## Overview
Here is a general overview of the RYVM virtual machine:

- RYVM is a 64-bit process virtual machine. It assumes all addresses are 64-bits and can store
  up to 64 bits in each of its registers.

### General Definitions
First, we will cover some terminology and definitions:
- A bit is a binary value (0 or 1)
- A byte is defined as 8 bits.
- A word is defined as 8 bytes.
- Bytecode is the binary instruction set that a virtual machine can understand and execute. 
- RYVM is a bytecode interpreter that interprets bytecode and executes the appropriate native code
  for that bytecode.
- RYVM programs can be handwritten using RYVM Assembly, a human-readable format for RYVM bytecode.
  - There programs can be stored in .ryasm files.
- RYVM Assembly files can be compiled into RYVM executable files (under the .ryc extension), which contain raw binary bytecode that the VM can execute.


### Registers
- There are 64 registers, each of which can hold a 64-bit word.
- You can access a register using an index of 0 through 63.
- Each register has a 8-bit, 16-bit, 32-bit, and 64-bit view.
  - E0 accesses the least significant byte of register 0.
  - Q1 accesses the 2 least significant bytes of register 1.
  - H10 accesses the 4 least significant bytes of register 10.
  - W25 accesses the full 8 byte value of register 25.

- Here is a diagram regarding what bytes are processed when 
  using the E, Q, H, and W sigils for registers:

```
Register Bytewidth | Least Significant Bytes                        Most Significant Bytes
  E (eigth word)   |-Byte 1-| 
  Q (quarter word) |-Byte 1-|-Byte 2-|
  H (half word)    |-Byte 1-|-Byte 2-|-Byte 3-|-Byte 4-|
  W (whole word)   |-Byte 1-|-Byte 2-|-Byte 3-|-Byte 4-|-Byte 5-|-Byte 6-|-Byte 7-|-Byte 8-|
```

- There are 59 general purpose registers (W0-W59). These store 64 bits of memory, which can
  be used to store 64-bit memory addresses, 32-bit and 64-bit floating-point numbers, signed and unsigned integers, fixed-point numbers, and any other data that can fit within 64 bits.

- There are currently five special registers:
  - PC 
    - The program counter. Stores the address of the next instruction to execute.
    - Is a alias of register W63
  - SP 
    - The stack pointer. Stores the address to the top of the VM's stack.
    - Is a alias of register W62
  - FP 
    - The frame pointer. Stores the value of the stack pointer the moment before a subroutine call is made.
    - Is a alias of register W61
  - LR
    - The link register. Used to store the return address of the caller code before entering a subroutine.
    - Is an alias of register W60
  - SF
    - The status register. Stores some flags relating to the state of the VM.
    - There are currently no specified flags yet. 
    - Is an alias of register W59

- Despite W59-W63 being "special" registers, you can still manipulate them like any other register.
  Just be warned that modifying their values may cause issues if you don't know the purpose of those
  registers.

### Stack
The RYVM virtual machine has a stack, whose size can be configured by the programmer of the RYVM
assembler file. Note that the stack does not grow at runtime, so the programmer must ensure that
the stack pointer never goes out-of-bounds.

### Memory Safety (or lack thereof)
- There are currently no checks to ensure memory safety, so if the stack overflows or the PC
  tries to execute instructions outside the .text section, undefined behavior will occur and 
  likely cause the VM to crash.
- I hope to change this in the future by allowing the option for the programmer to configure the VM
  to perform automatic memory safety checks.




## RYVM Assembly Instruction Set
There are currently 49 different instructions. All instructions are 4 bytes long. All instructions start with a 1-byte opcode.


### Formats
There are 4 formats that the instruction set can take: R0, R1, R2, and R3.

#### R0
> 0 register arguments, 1 24-bit immediate argument

The first byte is the opcode, and the other 3 bytes store a single
24-bit immediate value.

```
Bits:  31-24         23-0
  |-- Opcode --|-- 24-bit immediate value --| 
```

#### R1
> 1 register argument, 1 16-bit immediate value

The first byte is the opcode, the 2nd byte is the register and it's bytewidth, and the 3rd and 4th bytes represent a 16-bit immediate value.

```
Bits:  31-24              23-16                       15-0
  |-- Opcode --|-- Destination Register --|-- 16-bit immediate value --| 
```

#### R2
> 2 register arguments, 1 8-bit immediate value

The first byte is the opcode, the 2nd byte is the destination register, the 3rd byte is the source register, and the 4th byte represents a 8-bit immediate value.

```
Bits:  31-24              23-16                    15-8                       7-0
  |-- Opcode --|-- Destination Register --|-- Source Register --|-- 8-bit immediate value --| 
```

#### R3
> 3 register arguments, 0 immediate values

The first byte is the opcode, the 2nd byte is the destination register, the 3rd byte is the 1st source register, and the 4th byte represents the 2nd source register

```
Bits:  31-24              23-16                       15-8                       7-0
  |-- Opcode --|-- Destination Register --|-- 1st Source Register --|-- 2nd Source Register --| 
```

### Mnemonics
Note that there are no pseudo-instructions; Each mnemonic directly corresponds to a opcode inside RYVM.
Here is the current list of mnemonics that correspond to each opcode for RYVM:

```
LDA E0, W1, imm        ; Load from address at (W1 + imm) and put the 8 bits into E0
PCR W0, imm            ; Get PC-relative address using a 2-byte signed offset and store it in W0
LDI W0, imm            ; load 2-byte immediate value that's sign-extended to the specified byte-width. Can only be integers.
STR E0, W1, imm        ; Store E0 into address stored at (W1 + imm)
FXFP W0 W1 imm         ; Convert fixed point to floating point. W0 = (float) W1; where imm[0] determines if W1 is signed or unsigned, and imm[1:7] is the 7-bit number representing the number of fractional bits in W1 
FPFX W0 W1 imm         ; Convert floating point to fixed point. W0 = imm & 0b10000000 == 0 ? (unsigned int) W1 : int(W1); where imm[0] determines if W0 is signed or unsigned, and imm[1:7] is the 7-bit number representing the number of fractional bits in W0 
ADDI W0 W1 imm         ; W1 + imm = W0 ; imm is signed 8 bits
SUBI W0 W1 imm         ; W1 - imm = W0 ; imm is signed 8 bits
ADD E0, E1, E2         ; Add 8bit 2-s complement integers and store it in E0
SUB E0, E1, E2         ; Subtract 8bit 2-s complement integers and store it in E0
MUL E0, E1, E2         ; multiply 2 signed integers 
MULU E0, E1, E2        ; multiply 2 unsigned integers 
DIV E0, E1, E2         ; divide 2 signed integers 
DIVU E0, E1, E2        ; divide 2 unsigned integers 
REM E0, E1, E2         ; remainder 2 signed integers 
REMU E0, E1, E2        ; remainder 2 unsigned integers 
ADDF W0, W1, W2        ; Add 64bit floating point and store it in W0
SUBF W0, W1, W2        ; Subtract 64bit floating point and store it in W0
MULF W0, W1, W2        ; Multiply 64bit floating point and store it in W0
DIVF W0, W1, W2        ; Divide 64bit floating point and store it in W0
REMF W0, W1, W2        ; Remainder 64bit floating point and store it in W0
AND W0, W1, W2         ; bitwise AND operation
OR W0, W1, W2          ; bitwise OR operation
XOR W0, W1, W2         ; bitwise XOR operation
XORI W0, W1, imm      ; bitwise XOR operation with immediate 8bit value sign extended to bytewidth of source register
SHL W0, W1, W2         ; bitwise shift left
SHR W0, W1, W2         ; bitwise shift right
BIC E0 E1 E2           ; if bit at E2 is 0, keep bit at E1. If bit at E2 is 1, clear bit in E1 to 0. Store result in E0
EQ W0, W1, W2          ; W1 == W2, store result to W0
NE W0, W1, W2          ; W1 != W2, store result to W0
GTS W0, W1, W2         ; W1 signed integer is greater than W2 signed integer, store result to W0
LTS W0, W1, W2         ; W1 signed integer is less than W2 signed integer, store result to W0
GES W0, W1, W2         ; W1 signed integer is greater than or equal to W2 signed integer, store result to W0
LES W0, W1, W2         ; W1 signed integer is less than or equal to W2 signed integer, store result to W0
GTU W0, W1, W2         ; W1 unsigned integer is greater than W2 unsigned integer, store result to W0
LTU W0, W1, W2         ; W1 unsigned integer is less than W2 unsigned integer, store result to W0
GEU W0, W1, W2         ; W1 unsigned integer is greater than or equal to W2 unsigned integer, store result to W0
LEU W0, W1, W2         ; W1 unsigned integer is les than or equal to W2 unsigned integer, store result to W0
GTF W0, W1, W2         ; W1 float is greater than W2 float, store result to W0
LTF W0, W1, W2         ; W1 float is less than W2 float, store result to W0
GEF W0, W1, W2         ; W1 float is greater or equal to W2 float, store result to W0
LEF W0, W1, W2         ; W1 float is less or equal to W2 float, store result to W0
B imm                 ; unconditional jump to signed 24-bit PC-relative offset
BZ W0 imm             ; if W0 is zero, jump to PC + imm
BNZ W0 imm            ; if W0 is non-zero, jump to PC + imm
BR W0, imm            ; pc = W0 + imm;  indirect jump without saving link register. can be useful for return statement or for executing a specific function within an array of function pointers.
BL W0, imm            ; branch and link; W0 = pc + 4; pc = pc + imm  ; imm is signed 16bit offset
BLR W0, W1, imm       ; W0 = pc + 4;   pc = W1 + imm ; imm is signed 8bits (used for indirect jumps, calls, and returns)
SYS imm               ; a external function call to call OS-specific functions in a cross-platform way, using a 24bit syscall number 

```

## RYVM Assembly Syntax.
If you want examples of the current syntax of RYVM Assembly, look at the test/programs directory and check the .ryasm files. Here's a summary of the syntax:

### Comments
RYVM Assembly uses the ';' character to specify a single line comment. There are no block comments:
```
; Here is a comment
LDI W0 10  ; here is a comment after an instruction

```

### Labels
You can label instructions or data entries using the :label syntax. Each label represents a specific address within the RYVM code and data section. All label definitions start
with the ':' sigil and can be followed by any non-whitespace ASCII characters.
  - :begin
  - :1
  - :Hi_There_1

Note that you must follow a label by either an instruction or data entry.

 
### Label Expressions
Labels are useless without the ability to actually use them as expressions for immediate values
or literals within instructions or data entries.
There are 2 different ways to use a label as a expression for where immediate values or literals
would normally go.

1. Signed PC-relative Offsets (#label)
  - You can use the '#' sigil before a label name to specify that you want to insert a signed PC-relative offset at a specific instruction or data entry. 
  
  - Example
    ```
    .max_stack_size 0
    .data
    :data_entry_1 .word 8

    .text
    ;if we want to load the 8-byte value at :data_entry_1, we need to retrieve its offset
    ;from the PC and get its address.

    ;We can do this by using #data_entry_1, which will insert the PC-relative offset of :data_entry_1.
    ;Because the PCR instruction expects a 16-bit signed integer, the value of
    ; #data_entry_1 will be a 16-bit signed offset.

    PCR W0 #data_entry_1 ; get the signed PC-relative offset, add it to the PC, and store it in W0
    
    ; The above instruction gets converted to PCR W0 -12, since the start of :data_entry_1 is 
    ; -12 bytes away from the current PC. The PCR instruction will then add PC to -12 and store
    ; the result in W0
    
    ; Now W0 contains the result of PC + (PC-relative offset)

    LDA W0 W0 0  ; load address in W0 + 0 and store the value 8 in W0.

    ; Now W0 contains 8.

    ```

2. Address of Label (@label)
  - You can use the '@' sigil before a label name to specify that you want to insert the full
    8-byte address of the label.
  - Note that you should use the PC-relative offsets most of the time. However, depending on the instruction, you may only be able to store that offset in a 8-bit, 16-bit, or 24-bit value. If
  the offset is too large to fit in those values, the program will crash. 
    - By using the @label syntax, you can insert a literal pool near the instruction that needs
      the out-of-range address and make that instruction perform a PC-relative load to retrieve that
      address.
  - Example
    ```
    .max_stack_size 0
    .text

    ; note that you can insert data entries inside .text sections to create literal pools.
    ; However, you must ensure you jump over those literal pools to avoid executing them
    ; as actual code. This is similar to how ARM64 handles literal pools.

    B #begin   ; jump over literal pool to the :begin label.

    :func_adr .word @func   ;store :func full address in literal pool entry.

    :begin

    LDI W1 10 ; load immediate value 10 into W0
    
    PCR W0 #func_adr  ; get PC-relative offset of literal pool entry containing the :func address.
    LDA W0 W0 0       ;load the address of :func into W0
    
    BLR LR W0 0       ; branch to the address W0 + 0, storing the return address in LR.
    SYS 0             ; exit the VM


    :func
      ADDI W0 W1 8 ; add 8 to W1 and store it in W0
      BLR LR 0     ; return to address stored in link register


    ```

### Configuration
All RYVM Assembly files start with setting the configuration of the VM. Here are the current configuration options.
- .max_stack_size |num_bytes| 
  - This tells the VM how many bytes to allocate for the stack when executing this program.
  - You must use a non-negative integer literal for the |num_bytes| placeholder.
  

### .data section
After this, you can include an optional .data section to list all global data for the executable. 

#### Data Entry Types
Here are the following allowed data types:
- .eword
  - Specifies the start of a list of 8-bit integer values. You can use positive or negative integer
    literals here.
- .qword
  - Specifies the start of a list of 16-bit integer values. You can use positive or negative integer
    literals here.
- .hword
  - Specifies the start of a list of 32-bit integer values. You can use positive or negative integer
    literals here.
- .word
  - Specifies the start of a list of 64-bit integer values. You can use positive or negative integer
    literals here OR insert the address of a label using the @label syntax, which inserts the 
    64-bit relative address of the label.
- .asciz
  - Specifies a single ASCII string literal to insert, automatically appending the null character (\0)
    at the end of the string.

### .text section

After the .data section, you will insert a .text section, which contains the actual bytecode that
will run on the VM. Here, you can insert both instructions and data entries. Each instruction
contains a mnemonic followed by 1-3 arguments, which can either be registers or immediate values.

Example:
```
.text

; Instructions
LDI W0 10  ;load immediate signed value 10 into W0
LDI W1 5  ;load immediate signed value 10 into W0

ADD W2 W0 W1 ; W2 = W0 + W1 = 10 + 5 = 15

ADDI W2 W2 5 ; W2 = W2 + 5 = 15 + 5 = 20  

B #end_literal_pool  ;jump over literal pool data entries to avoid executing them.

;literal pools can be used to store memory addresses that cannot be reached by a PC-relative offset.
.word 8
.word 10
.asciz "Hi there"

:end_literal_pool

SYS 0  ; a syscall with 24-bit immediate value 0. This exits the VM.
```

## RYVM Executable Format
Due to this project being a work-in-progress, the format of the RYVM executable is rapidly changing
and has no documentation. Assume that a RYVM executable you compiled in a previous version of
RYVM will not work on a newer version.

Once I finish all of the main features of the VM, I will document the file format for the RYVM Executable.

## Similar Projects
- Java Virtual Machine
- Lua virtual machine