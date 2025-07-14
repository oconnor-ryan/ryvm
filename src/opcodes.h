#ifndef RYVM_OPCODE_H
#define RYVM_OPCODE_H

#include <stdint.h>

//Note: changing the order of these opcodes will change the numeric value of the opcode
//itself. Be aware of this when altering the order of these opcodes.
enum ryvm_opcode {
  RYVM_OP_LDA,     // LDA E0, W1, #off       ; Load from address at (W1 + off) and put the 8 bits into E0
  RYVM_OP_PCR,     // PCR W0, #1             ; Get PC-relative address using a 2-byte signed offset and store it in W0
  RYVM_OP_LDI,     // LDI W0, #1             ; load 2-byte immediate value that's sign-extended to the specified byte-width. Can only be integers.
  RYVM_OP_STR,     // STR E0, W1, #off       ; Store E0 into address stored at W1 + off
  RYVM_OP_FXFP,    // FXFP W0 W1 #fixed_prec ; W0 = (float) W1; where #fixed_prec[0] determines if W1 is signed or unsigned, and #fixed_prec[1:7] is the 7-bit number representing the number of fractional bits in W1 
  RYVM_OP_FPFX,    // FPFX W0 W1 #fixed_prec ; W0 = #fixed_prec & 0b10000000 == 0 ? (unsigned int) W1 : int(W1); where #fixed_prec[0] determines if W0 is signed or unsigned, and #fixed_prec[1:7] is the 7-bit number representing the number of fractional bits in W0 
  RYVM_OP_ADDI,    // ADDI W0 W1 #imm        ; W0 = W1 + imm ; imm is signed 8 bits
  RYVM_OP_SUBI,    // SUBI W0 W1 #imm        ; W0 = W1 - imm; imm is signed 8 bits
  RYVM_OP_ADD ,    // ADD E0, E1, E2         ; Add 8bit 2-s complement integers and store it in E0
  RYVM_OP_SUB ,    // SUB E0, E1, E2         ; Subtract 8bit 2-s complement integers and store it in E0
  RYVM_OP_MUL ,    // MUL E0, E1, E2         ; multiply 2 signed integers 
  RYVM_OP_MULU,    // MULU E0, E1, E2        ; multiply 2 unsigned integers 
  RYVM_OP_DIV ,    // DIV E0, E1, E2         ; divide 2 signed integers 
  RYVM_OP_DIVU,    // DIVU E0, E1, E2        ; divide 2 unsigned integers 
  RYVM_OP_REM ,    // REM E0, E1, E2         ; remainder 2 signed integers 
  RYVM_OP_REMU ,   // REMU E0, E1, E2        ; remainder 2 unsigned integers 
  RYVM_OP_ADDF ,   // ADDF W0, W1, W2        ; Add 64bit floating point and store it in W0
  RYVM_OP_SUBF ,   // SUBF W0, W1, W2        ; Subtract 64bit floating point and store it in W0
  RYVM_OP_MULF ,   // MULF W0, W1, W2        ; Multiply 64bit floating point and store it in W0
  RYVM_OP_DIVF ,   // DIVF W0, W1, W2        ; Divide 64bit floating point and store it in W0
  RYVM_OP_REMF ,   // REMF W0, W1, W2        ; Remainder 64bit floating point and store it in W0
  RYVM_OP_AND,     // AND W0, W1, W2         ; bitwise AND operation
  RYVM_OP_OR ,     // OR W0, W1, W2          ; bitwise OR operation
  RYVM_OP_XOR ,    // XOR W0, W1, W2         ; bitwise XOR operation
  RYVM_OP_XORI,    // XORI W0, W1, #imm      ; bitwise XOR operation with immediate 8bit value sign extended to bytewidth of source register
  RYVM_OP_SHL ,    // SHL W0, W1, W2         ; bitwise shift left
  RYVM_OP_SHR ,    // SHR W0, W1, W2         ; bitwise shift right
  RYVM_OP_BIC ,    // BIC E0 E1 E2           ; if bit at E2 is 0, keep bit at E1. If bit at E2 is 1, clear bit in E1 to 0. Store result in E0
  RYVM_OP_CPS ,    // CPS W0 W1 W2           ; Signed comparison; Similar to SUB, except SF flags register is updated (specifically the N,Z, and V flags)
  RYVM_OP_CPU ,    // CPU W0 W1 W2           ; Unsigned comparison; Similar to SUB, except SF flags register is updated (specifically the N,Z, and V flags)
  RYVM_OP_CPF ,    // CPF W0 W1 W2           ; Floating point comparison; Similar to SUBF, except SF flags register is updated (specifically the N,Z, and V flags)
  RYVM_OP_CPSI ,   // CPSI W0 #imm           ; Signed comparison to 16-bit immediate; Result is discarded, and SF flags register is updated (specifically the N,Z, and V flags)
  RYVM_OP_CPUI ,   // CPUI W0 #imm           ; Unsigned comparison to 16-bit immediate; Result is discarded, and SF flags register is updated (specifically the N,Z, and V flags)
  RYVM_OP_B ,      // B #imm                 ; unconditional jump to signed 24-bit PC-relative offset
  RYVM_OP_BEQ,     // BEQ #imm               ; Check status register if previous comparison (CPS, CPU, CPF) states that both operands were equal, if this is true, jump to PC-relative offset.
  RYVM_OP_BNE,     // BNE #imm               ; Check status register if previous comparison (CPS, CPU, CPF) states that both operands were not equal, if this is true, jump to PC-relative offset.
  RYVM_OP_BLT,     // BLT #imm               ; Check status register if previous comparison (CPS, CPU, CPF) states that operand1 < operand2, if this is true, jump to PC-relative offset.
  RYVM_OP_BGT,     // BGT #imm               ; Check status register if previous comparison (CPS, CPU, CPF) states that operand1 > operand2, if this is true, jump to PC-relative offset.
  RYVM_OP_BLE,     // BLE #imm               ; Check status register if previous comparison (CPS, CPU, CPF) states that operand1 <= operand2, if this is true, jump to PC-relative offset.
  RYVM_OP_BGE,     // BGE #imm               ; Check status register if previous comparison (CPS, CPU, CPF) states that operand1 >= operand2, if this is true, jump to PC-relative offset.
  RYVM_OP_BR,      // BR W0, #imm            ; pc = W0 + #imm;  indirect jump without saving link register. can be useful for return statement or for executing a specific function within an array of function pointers.
  RYVM_OP_BL ,     // BL W0, #imm            ; branch and link; W0 = pc + 4; pc = pc + imm  ; imm is signed 16bit offset
  RYVM_OP_BLR ,    // BLR W0, W1, #imm       ; W0 = pc + 4;   pc = W1 + imm ; imm is signed 8bits (used for indirect jumps, calls, and returns)
  RYVM_OP_SYS      // SYS #imm               ; a external function call to call OS-specific functions in a cross-platform way, using a 24bit syscall number 
};

enum ryvm_ins_format {
  RYVM_INS_FORMAT_R0, // 0 registers, 1 24-bit immediate value
  RYVM_INS_FORMAT_R1, // 1 registers, 1 16-bit immediate value 
  RYVM_INS_FORMAT_R2, // 2 registers, 1 8-bit immediate value
  RYVM_INS_FORMAT_R3, // 3 registers, 0 immediate values
};





int ryvm_opcode_str_to_op(const char *s, enum ryvm_opcode *code);
const char * ryvm_opcode_op_to_str(enum ryvm_opcode op);
enum ryvm_ins_format ryvm_opcode_get_ins_format(enum ryvm_opcode op);


#endif// RYVM_OPCODE_H

