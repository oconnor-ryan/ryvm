
//wrap param in double quotes:
// EX: RYVM(string) becomes "string"
#define RYVM_STR(x) #x

//concatenate 2 tokens
//Ex: RYVM_CON(HI_THERE_, BOB) becomes HI_THERE_BOB
#define RYVM_CON(a,b) a##b

#define RYVM_PC_REG 63
#define RYVM_SP_REG 61
#define RYVM_FP_REG 60
#define RYVM_SR_REG 62  //status register (similar to RFLAGS on x86-64 and CPSR on ARM64)

#define RYVM_INS_SIZE 4
