#ifndef RYVM_HELPER_H
#define RYVM_HELPER_H

#include <stdint.h>

//wrap param in double quotes:
// EX: RYVM(string) becomes "string"
#define RYVM_STR(x) #x

//concatenate 2 tokens
//Ex: RYVM_CON(HI_THERE_, BOB) becomes HI_THERE_BOB
#define RYVM_CON(a,b) a##b

#define RYVM_PC_REG 63  // program counter
#define RYVM_SP_REG 62  // stack pointer
#define RYVM_FP_REG 61  // frame pointer
#define RYVM_LR_REG 60  // link register
#define RYVM_SF_REG 59  // status flag register (similar to RFLAGS on x86-64 and CPSR on ARM64)


#define RYVM_INS_SIZE 4


float ryvm_vm_helper_reg_to_float(uint64_t reg_value, uint8_t bytewidth);


double ryvm_vm_helper_reg_to_double(uint64_t reg_value, uint8_t bytewidth);




int64_t ryvm_vm_helper_sign_extend_64(uint8_t *bytes, uint8_t num_bytes);
int32_t ryvm_vm_helper_cast_int_24_to_32(uint8_t bytes[3]);



#endif // RYVM_HELPER_H

