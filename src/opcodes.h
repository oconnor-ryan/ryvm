#ifndef RYVM_OPCODE_H
#define RYVM_OPCODE_H

#include <stdint.h>

//Note: you're dead if you change the order of these.
enum ryvm_opcode {
  RYVM_OP_NOP,     // move value from 1 register to another.
  RYVM_OP_MOV,     // move value from 1 register to another.
  RYVM_OP_LDR,     // Load from address at O1 the 8 bits into B0
  RYVM_OP_PCA,     // Get PC-relative address using a 2-byte signed offset and store it in W0
  RYVM_OP_LDI,     // load 2-byte immediate value that's sign-extended to the specified byte-width. Can only be integers.
  RYVM_OP_STR,     // Store B0 into address stored at O1
  RYVM_OP_PUSH,    // Push B0 on top of stack
  RYVM_OP_POP ,    // Pop 8bits off top of stack and store it in B0
  RYVM_OP_ADD ,    // Add 8bit 2-s complement integers and store it in B0
  RYVM_OP_SUB ,    // Subtract 8bit 2-s complement integers and store it in B0
  RYVM_OP_MUL ,    // multiply 2 signed integers 
  RYVM_OP_MULU,    // multiply 2 unsigned integers 
  RYVM_OP_DIV ,    // divide 2 signed integers 
  RYVM_OP_DIVU,    // divide 2 unsigned integers 
  RYVM_OP_MOD ,    // remainder 2 signed integers 
  RYVM_OP_MODU ,   // remainder 2 unsigned integers 
  RYVM_OP_ADDF ,   // Add 64bit floating point and store it in O0
  RYVM_OP_SUBF ,   // Subtract 64bit floating point and store it in O0
  RYVM_OP_MULF ,   // Multiply 64bit floating point and store it in O0
  RYVM_OP_DIVF ,   // Divide 64bit floating point and store it in O0
  RYVM_OP_MODF ,   // Remainder 64bit floating point and store it in O0
  RYVM_OP_AND,     // bitwise AND operation
  RYVM_OP_OR ,     // bitwise OR operation
  RYVM_OP_XOR ,    // bitwise XOR operation
  RYVM_OP_NOT ,    // bitwise NOT
  RYVM_OP_SHL ,    // bitwise shift left
  RYVM_OP_SHR ,    // bitwise shift right
  RYVM_OP_BIC ,    // clear all bits to 0 for register
  RYVM_OP_EQ ,     // if 2 integers are equal
  RYVM_OP_NE ,     // if 2 integers are not equal
  RYVM_OP_GTS ,    // if left signed integer is greater than right signed integer
  RYVM_OP_LTS ,    // if left signed integer is less than right signed integer
  RYVM_OP_GES ,    // if left signed integer is greater or equal to right signed integer
  RYVM_OP_LES ,    // if left signed integer is less or equal to right signed integer
  RYVM_OP_GTU ,    // if left unsigned integer is greater than right unsigned integer
  RYVM_OP_LTU ,    // if left unsigned integer is less than right unsigned integer
  RYVM_OP_GEU ,    // if left unsigned integer is greater or equal to right unsigned integer
  RYVM_OP_LEU ,    // if left unsigned integer is less or equal to right unsigned integer
  RYVM_OP_EQF ,    // if 2 floats are equal
  RYVM_OP_NEF ,    // if 2 floats are not equal
  RYVM_OP_GTF ,    // if left float is greater than right float
  RYVM_OP_LTF ,    // if left float is less than right float
  RYVM_OP_GEF ,    // if left float is greater or equal to right float
  RYVM_OP_LEF ,    // if left float is less or equal to right float
  RYVM_OP_JMP ,    // unconditional jump to address at a register
  RYVM_OP_JMPF ,   // jump if last boolean comparison was false to address at a register
  RYVM_OP_JMPT ,   // jump if last boolean comparision was true to address at a register
  RYVM_OP_CALL ,   // push stack pointer to stack, then jump to address at a register
  RYVM_OP_RET,     // pop previous stack pointer and return to it.
  RYVM_OP_END ,    // Kill the VM immediately and return a signed byte exit code
  RYVM_OP_SYS // a external function call to call OS-specific functions in a cross-platform way. 
};


//config
extern const char *RYVM_CONFIG_MAX_STACK;
//code
extern const char *RYVM_CODE_HEADER;

//data
extern const char *RYVM_CONST_DATA_HEADER;
extern const char *RYVM_DATA_HEADER;
extern const char *RYVM_DATA_BYTE_HEADER;
extern const char *RYVM_DATA_2BYTE_HEADER;
extern const char *RYVM_DATA_4BYTE_HEADER;
extern const char *RYVM_DATA_8BYTE_HEADER;

extern const char *RYVM_DATA_2BYTE_FLOAT_HEADER;
extern const char *RYVM_DATA_4BYTE_FLOAT_HEADER;
extern const char *RYVM_DATA_8BYTE_FLOAT_HEADER;

extern const char *RYVM_DATA_ASCIZ_HEADER;


int ryvm_opcode_str_to_op(const char *s, enum ryvm_opcode *code);
const char * ryvm_opcode_op_to_str(enum ryvm_opcode op);

uint8_t ryvm_opcode_get_arity(enum ryvm_opcode op);


#endif// RYVM_OPCODE_H

