#ifndef RYVM_VM_H
#define RYVM_VM_H



#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#include "memory/memory.h"
#include "opcodes.h"

enum ryvm_num_type {
  RYVM_INT_TYPE_UINT8,
  RYVM_INT_TYPE_UINT16,
  RYVM_INT_TYPE_UINT32,
  RYVM_INT_TYPE_UINT64,
  RYVM_INT_TYPE_SINT8,
  RYVM_INT_TYPE_SINT16,
  RYVM_INT_TYPE_SINT32,
  RYVM_INT_TYPE_SINT64,

  RYVM_INT_TYPE_FLOAT32,
  RYVM_INT_TYPE_FLOAT64
};

struct ryvm {
  uint8_t *data_and_code;
  uint64_t data_and_code_size;

  uint64_t text_section_start;

  //general registers
  uint64_t gen_registers[64];



  uint8_t *stack;
  uint64_t stack_size;

  uint8_t is_running;

};

enum ryvm_vm_status_flag {
  RYVM_VM_STATUS_FLAG_N = 1,  //negative result flag
  RYVM_VM_STATUS_FLAG_V = 2,  //overflow flag (signed or unsigned)
  RYVM_VM_STATUS_FLAG_Z = 4,  //zero flag
};

//force these functions to be inline to minimize overhead during runtime.
extern inline uint64_t ryvm_vm_stack_ptr(struct ryvm *vm);
extern inline uint64_t ryvm_vm_frame_ptr(struct ryvm *vm);
extern inline uint64_t ryvm_vm_pc(struct ryvm *vm);
extern inline uint64_t ryvm_vm_flags(struct ryvm *vm);
extern inline uint64_t ryvm_vm_lr(struct ryvm *vm);


extern inline void ryvm_vm_stack_ptr_set(struct ryvm *vm, uint64_t new_val);
extern inline void ryvm_vm_frame_ptr_set(struct ryvm *vm, uint64_t new_val);
extern inline void ryvm_vm_pc_set(struct ryvm *vm, uint64_t new_val);
extern inline void ryvm_vm_pc_inc(struct ryvm *vm);

extern inline void ryvm_vm_flags_set(struct ryvm *vm, uint64_t new_val);
extern inline void ryvm_vm_flags_set_flag(struct ryvm *vm, uint8_t val, enum ryvm_vm_status_flag flag);

extern inline void ryvm_vm_lr_set(struct ryvm *vm, uint64_t new_val);
extern inline void ryvm_vm_byte_to_reg(uint8_t reg, uint8_t *reg_bytewidth, uint8_t *reg_num);


int ryvm_vm_load(struct ryvm *vm, FILE *input);
int64_t ryvm_vm_run(struct ryvm *vm);
void ryvm_vm_free(struct ryvm *vm);


#endif// RYVM_VM_H
