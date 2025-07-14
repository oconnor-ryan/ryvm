#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <fenv.h> //used to check for floating point overflow and other exceptions


#include "../helper.h"
#include "vm.h"


/*
  Currently, we are using REAL memory addresses in order to reference
  constants and instructions.

  An issue with this strategy is that: 
  1. Program will segfault, cause undefined behavior, and crash
  2. Its more difficult to track the program counter and perform jumps,
     since we have to keep switching between real and virtual addresses.

  We need to pick one or the other to keep things simple. 

  It's better to use virtual addresses to avoid leaky abstractions, but its harder
  to set up the environment.



*/

enum ryvm_vm_arith_op {
  RYVM_VM_ARITH_OP_ADD,
  RYVM_VM_ARITH_OP_SUB,
  RYVM_VM_ARITH_OP_MUL,
  RYVM_VM_ARITH_OP_DIV,
  RYVM_VM_ARITH_OP_REM,
  RYVM_VM_ARITH_OP_SHL,
  RYVM_VM_ARITH_OP_SHR,
  RYVM_VM_ARITH_OP_AND,
  RYVM_VM_ARITH_OP_OR,
  RYVM_VM_ARITH_OP_XOR,
};

inline void ryvm_vm_byte_to_reg(uint8_t reg, uint8_t *reg_bytewidth, uint8_t *reg_num) {
  //registers are little endian, so they look like this in memory
  /*
        LSB                        MSB
    E |--------| 
    Q |--------|--------|
    H |--------|--------|--------|--------|
    W |--------|--------|--------|--------|--------|--------|--------|--------|

  */
  //end bounds offset
  //E offset - 1
  //Q offset - 2
  //H offset - 4
  //W offset - 8

  *reg_bytewidth = reg >> 6; // shift away LSB so that only last 2 bytewidth bits are found

  //since possible values of the MSB 2 bits are  0,1,2,3, we need to 
  //calculate the byte_width using 2^(r >> 6)
  *reg_bytewidth = 1 << *reg_bytewidth; //we can use bitshifts instead of exponents since base is 2.

  
  *reg_num = reg & 63;   //0b00111111  // remove unneeded bitwidth bits
}

inline uint64_t ryvm_vm_frame_ptr(struct ryvm *vm) {return vm->gen_registers[RYVM_FP_REG];}
inline uint64_t ryvm_vm_stack_ptr(struct ryvm *vm) {return vm->gen_registers[RYVM_SP_REG];}
inline uint64_t ryvm_vm_flags(struct ryvm *vm)     {return vm->gen_registers[RYVM_SF_REG];}
inline uint64_t ryvm_vm_lr(struct ryvm *vm)        {return vm->gen_registers[RYVM_LR_REG];}
inline uint64_t ryvm_vm_pc(struct ryvm *vm)        {return vm->gen_registers[RYVM_PC_REG];}
inline void ryvm_vm_pc_inc(struct ryvm *vm)        {vm->gen_registers[RYVM_PC_REG] += RYVM_INS_SIZE;} //size of instruction is 4 bytes


inline void ryvm_vm_frame_ptr_set(struct ryvm *vm, uint64_t new_val)  {vm->gen_registers[RYVM_FP_REG] = new_val;}
inline void ryvm_vm_stack_ptr_set(struct ryvm *vm, uint64_t new_val)  {vm->gen_registers[RYVM_SP_REG] = new_val;}
inline void ryvm_vm_flags_set(struct ryvm *vm, uint64_t new_val)      {vm->gen_registers[RYVM_SF_REG] = new_val;}
inline void ryvm_vm_pc_set(struct ryvm *vm, uint64_t new_val)         {vm->gen_registers[RYVM_PC_REG] = new_val;}
inline void ryvm_vm_lr_set(struct ryvm *vm, uint64_t new_val)         {vm->gen_registers[RYVM_LR_REG] = new_val;}


inline void ryvm_vm_flags_set_flag(struct ryvm *vm, uint8_t val, enum ryvm_vm_status_flag flag) {
  if(val) {
    vm->gen_registers[RYVM_SF_REG] |= flag; //sets
  } else {
    vm->gen_registers[RYVM_SF_REG] &= ~flag; //clears
  }
}

int ryvm_vm_load(struct ryvm *vm, FILE *in) {
  char magic[2];
  fread(magic, 2, 1, in);

  if(magic[0] != 'R' || magic[1] != 'Y') {
    printf("Invalid file! Not a RyVM bytecode file!\n");
    return 0;
  }

  //read max_stack_size
  fread(&vm->stack_size, 8, 1, in);

  if(vm->stack_size != 0) {
  //fine to use malloc since size of memory will never grow or shrink
    vm->stack = malloc(vm->stack_size);
    if(vm->stack == NULL) {
      printf("Cannot allocate enough memory for stack!");
      return 0;
    }
  } else {
    vm->stack = NULL;
  }

  

  //read size of data section
  uint64_t data_size;
  fread(&data_size, 8, 1, in);

  

  vm->text_section_start = data_size;

  //load data into data_and_code memory block if there is data
  if(data_size != 0) {
    vm->data_and_code = malloc(data_size);
    if(vm->data_and_code == NULL) {
      printf("Cannot allocate enough memory for data!");
      return 0;
    }
    //put data at beginning
    fread(vm->data_and_code, data_size, 1, in);
  } 

  uint64_t text_size;
  fread(&text_size, 8, 1, in);

  if(data_size != 0) {
    uint8_t *tmp = realloc(vm->data_and_code, data_size + text_size);
    if(tmp == NULL) {
      free(vm->data_and_code);
      printf("Cannot allocate enough memory for data!");
      return 0;
    }

    vm->data_and_code = tmp;
  } else {
    vm->data_and_code = malloc(text_size);
    if(vm->data_and_code == NULL) {
      printf("Cannot allocate enough memory for data!");
      return 0;
    }
  }

  //read text section into memory
  fread(vm->data_and_code + data_size, text_size, 1, in);

  uint64_t num_reloc_entries;
  fread(&num_reloc_entries, 8, 1, in);



  // read the relocation entries and change the relative address values to
  // true in-memory addresses
  for(uint64_t i = 0; i < num_reloc_entries; i++) {
    uint64_t reloc_relative_address_hole;
    uint64_t reloc_relative_address_value;

    fread(&reloc_relative_address_hole, 8, 1, in);
    fread(&reloc_relative_address_value, 8, 1, in);

    uint64_t true_address_of_value = (uint64_t) (vm->data_and_code + reloc_relative_address_value);

    uint64_t *hole = (uint64_t*) (vm->data_and_code + reloc_relative_address_hole);
    *hole = true_address_of_value; 
  }


  //


  return 1;
}


void ryvm_vm_unsigned_int_arith(struct ryvm *vm, uint8_t reg1_num, uint8_t reg1_bytewidth, uint8_t reg2_num, uint8_t reg2_bytewidth, uint8_t reg3_num, uint8_t reg3_bytewidth, enum ryvm_vm_arith_op op) {
  //perform zero extension
  uint64_t a = 0; 
  uint64_t b = 0; 
  //copy value to temporary vars, while also zero-extending them to 64 bits
  memcpy(&a, &vm->gen_registers[reg2_num], reg2_bytewidth); 
  memcpy(&b, &vm->gen_registers[reg3_num], reg3_bytewidth); 

  uint64_t value;
  switch(op) {
    case RYVM_VM_ARITH_OP_ADD: value = a + b; break;
    case RYVM_VM_ARITH_OP_SUB: value = a - b; break;
    case RYVM_VM_ARITH_OP_MUL: value = a * b; break;
    case RYVM_VM_ARITH_OP_DIV: value = a / b; break;
    case RYVM_VM_ARITH_OP_REM: value = a % b; break;
    case RYVM_VM_ARITH_OP_SHL: value = a << b; break;
    case RYVM_VM_ARITH_OP_SHR: value = a >> b; break;
    case RYVM_VM_ARITH_OP_AND: value = a & b; break;
    case RYVM_VM_ARITH_OP_OR: value = a | b; break;
    case RYVM_VM_ARITH_OP_XOR: value = a ^ b; break;
    default: assert(0);
  }

  //only save the number of bytes specified in the bytewidth 
  memcpy(&vm->gen_registers[reg1_num], &value, reg1_bytewidth); 
}

void ryvm_vm_signed_int_arith(struct ryvm *vm, uint8_t reg1_num, uint8_t reg1_bytewidth, uint8_t reg2_num, uint8_t reg2_bytewidth, uint8_t reg3_num, uint8_t reg3_bytewidth, enum ryvm_vm_arith_op op) {
  int64_t a = ryvm_vm_helper_sign_extend_64((uint8_t*) &vm->gen_registers[reg2_num], reg2_bytewidth); 
  int64_t b = ryvm_vm_helper_sign_extend_64((uint8_t*) &vm->gen_registers[reg3_num], reg3_bytewidth); 
  int64_t value;
  switch(op) {
    case RYVM_VM_ARITH_OP_ADD: value = a + b; break;
    case RYVM_VM_ARITH_OP_SUB: value = a - b; break;
    case RYVM_VM_ARITH_OP_MUL: value = a * b; break;
    case RYVM_VM_ARITH_OP_DIV: value = a / b; break;
    case RYVM_VM_ARITH_OP_REM: value = a % b; break;
    case RYVM_VM_ARITH_OP_SHL: value = a << b; break;
    case RYVM_VM_ARITH_OP_SHR: value = a >> b; break;
    case RYVM_VM_ARITH_OP_AND: value = a & b; break;
    case RYVM_VM_ARITH_OP_OR: value = a | b; break;
    case RYVM_VM_ARITH_OP_XOR: value = a ^ b; break;
    default: assert(0);
  }

  memcpy(&vm->gen_registers[reg1_num], &value, reg1_bytewidth); 
}

void ryvm_vm_float_arith(struct ryvm *vm, uint8_t reg1_num, uint8_t reg1_bytewidth, uint8_t reg2_num, uint8_t reg2_bytewidth, uint8_t reg3_num, uint8_t reg3_bytewidth, enum ryvm_vm_arith_op op) {
  uint8_t largest_bytewidth = reg2_bytewidth > reg3_bytewidth ? reg2_bytewidth : reg3_bytewidth;

  uint64_t result;

  //we we perform a 64-bit floating point operation
  if(largest_bytewidth > 4) {
    double a = ryvm_vm_helper_reg_to_double(vm->gen_registers[reg2_num], reg2_bytewidth);
    double b = ryvm_vm_helper_reg_to_double(vm->gen_registers[reg3_num], reg3_bytewidth);

    double value;
    switch(op) {
      case RYVM_VM_ARITH_OP_ADD: value = a + b; break;
      case RYVM_VM_ARITH_OP_SUB: value = a - b; break;
      case RYVM_VM_ARITH_OP_MUL: value = a * b; break;
      case RYVM_VM_ARITH_OP_DIV: value = a / b; break;
      case RYVM_VM_ARITH_OP_REM: value = fmod(a, b); break;
      default: assert(0);
    }

    memcpy(&result, &value, 8);
  } 
  //TODO: Implement 16-bit IEEE 754 floating point and find a 8-bit floating point.
  else {
    float a = ryvm_vm_helper_reg_to_float(vm->gen_registers[reg2_num], reg2_bytewidth);
    float b = ryvm_vm_helper_reg_to_float(vm->gen_registers[reg3_num], reg3_bytewidth);

    float value;
    switch(op) {
      case RYVM_VM_ARITH_OP_ADD: value = a + b; break;
      case RYVM_VM_ARITH_OP_SUB: value = a - b; break;
      case RYVM_VM_ARITH_OP_MUL: value = a * b; break;
      case RYVM_VM_ARITH_OP_DIV: value = a / b; break;
      case RYVM_VM_ARITH_OP_REM: value = fmod(a, b); break;
      default: assert(0);
    }

    memcpy(&result, &value, 4);
  }

  //note that depending on the bytewidth of the result register, we may need to 
  //cast the original result from a float to a double or vise versa.

  if(reg1_bytewidth > 4) {
    double res = ryvm_vm_helper_reg_to_double(result, largest_bytewidth);
    memcpy(vm->gen_registers+reg1_num, &res, 8);
  } else {
    float res = ryvm_vm_helper_reg_to_float(result, largest_bytewidth);
    memcpy(vm->gen_registers+reg1_num, &res, 4);
  }
  
}

int64_t ryvm_vm_run(struct ryvm *vm) {
  //ryvm_vm_pc_set(vm, 0);
  ryvm_vm_pc_set(vm, (uint64_t) (vm->data_and_code + vm->text_section_start));
  ryvm_vm_flags_set(vm, 0);
  ryvm_vm_stack_ptr_set(vm, (uint64_t) vm->stack);
  ryvm_vm_frame_ptr_set(vm, (uint64_t) vm->stack);

  vm->is_running = 1;


  int64_t result = -1;

  

  while(vm->is_running) {
    //struct ryvm_ins ins = vm->instructions[ryvm_vm_pc(vm)];

    uint8_t *ins = (uint8_t*) ryvm_vm_pc(vm);

    ryvm_vm_pc_inc(vm); //increment PC.


    enum ryvm_opcode op = (enum ryvm_opcode) ins[0];
    uint8_t reg1_bytewidth;
    uint8_t reg1_num;
    uint8_t reg2_bytewidth;
    uint8_t reg2_num;
    uint8_t reg3_bytewidth;
    uint8_t reg3_num;

    ryvm_vm_byte_to_reg(ins[1], &reg1_bytewidth, &reg1_num);
    ryvm_vm_byte_to_reg(ins[2], &reg2_bytewidth, &reg2_num);
    ryvm_vm_byte_to_reg(ins[3], &reg3_bytewidth, &reg3_num);


    switch(op) {
      /* Conversions */
      case RYVM_OP_FPFX: {
        //TODO, add algorithm to convert floating point to fixed point number
        uint8_t is_signed = ins[3] & 128; // extract most significant bit
        //uint8_t fixed_point_frac_precision = ins[3] & 127; //extract 7 least significant bits

        if(reg2_bytewidth <= 4) {
          float *old_value = (float*) vm->gen_registers + reg2_num;
          if(is_signed) {
            int64_t converted_value = (int64_t) *old_value;
            memcpy(&vm->gen_registers[reg1_num], &converted_value, reg1_bytewidth);
          } else {
            uint64_t converted_value = (uint64_t) *old_value;
            memcpy(&vm->gen_registers[reg1_num], &converted_value, reg1_bytewidth);
          }
        } else {
          double *old_value = (double*) vm->gen_registers + reg2_num;
          if(is_signed) {
            int64_t converted_value = (int64_t) *old_value;
            memcpy(&vm->gen_registers[reg1_num], &converted_value, reg1_bytewidth);
          } else {
            uint64_t converted_value = (uint64_t) *old_value;
            memcpy(&vm->gen_registers[reg1_num], &converted_value, reg1_bytewidth);
          }
        }

        break;
      }

      case RYVM_OP_FXFP: {
        //TODO, add algorithm to convert fixed point to floating point number
        uint8_t is_signed = ins[3] & 128; // extract most significant bit
        //uint8_t fixed_point_frac_precision = ins[3] & 127; //extract 7 least significant bits

        if(reg2_bytewidth <= 4) {
          if(is_signed) {
            int64_t *signed_value = (int64_t*) vm->gen_registers[reg2_num];
            float converted_value = (float) *signed_value;
            memcpy(&vm->gen_registers[reg1_num], &converted_value, reg1_bytewidth);
          } else {
            float converted_value = (float) vm->gen_registers[reg2_num];
            memcpy(&vm->gen_registers[reg1_num], &converted_value, reg1_bytewidth);
          }
        } else {
          if(is_signed) {
            int64_t *signed_value = (int64_t*) vm->gen_registers[reg2_num];
            double converted_value = (double) *signed_value;
            memcpy(&vm->gen_registers[reg1_num], &converted_value, reg1_bytewidth);
          } else {
            double converted_value = (double) vm->gen_registers[reg2_num];
            memcpy(&vm->gen_registers[reg1_num], &converted_value, reg1_bytewidth);
          }
        }

        break;
      }

      /* move, load, and store */
      case RYVM_OP_PCR: {
        uint8_t *dest = (uint8_t*) &vm->gen_registers[reg1_num];

        int16_t *offset = (int16_t*) (ins + 2);
        uint64_t val = ryvm_vm_pc(vm) + *offset;

        uint8_t end_offset_dest = reg1_bytewidth;
        memcpy(dest, &val, end_offset_dest);

        break;
      }
      case RYVM_OP_LDA: {
        uint8_t *dest = (uint8_t*) &vm->gen_registers[reg1_num];
        int8_t offset = ins[3];
        
        //attempt to dereference address inside register.
        //This is a REAL address, not one that is controlled by the virtual
        //machine.
        //THIS WILL CAUSE UNDEFINED BEHAVIOR if this is not a valid address.
      //  uint64_t value = *((uint64_t*) (vm->gen_registers[reg2_num] + offset));

        //you may choose the bytewidth of the destination register, but 
        //the src register's bytewidth does not matter. The src register will
        //always be read in its entirety
        memcpy(dest, (void*) (vm->gen_registers[reg2_num] + offset), reg1_bytewidth);

        break;
      }

      case RYVM_OP_LDI: {
        //in order to sign-extend a value, C requires the smaller
        //value to be cast to a SIGNED datatype. 
        //sign-extend 16-bit value to 64 bits
        int64_t val = (int64_t) *((int16_t*) (ins+2));
        memcpy(&vm->gen_registers[reg1_num], &val, reg1_bytewidth);
        break;
      }

      //store value at a memory address
      case RYVM_OP_STR: {
        //remember that THIS WILL CAUSE UNDEFINED BEHAVIOR if the address
        //in this register is invalid.
        int8_t offset = ins[3];
        uint64_t *dest_address = (uint64_t*)(vm->gen_registers[reg2_num] + offset);
        memcpy(dest_address, &vm->gen_registers[reg1_num], reg1_bytewidth);
        break;
      }

      /* Bitwise operations */
      case RYVM_OP_AND: ryvm_vm_unsigned_int_arith(vm, reg1_num, reg1_bytewidth, reg2_num, reg2_bytewidth, reg3_num, reg3_bytewidth, RYVM_VM_ARITH_OP_AND); break;
      case RYVM_OP_OR: ryvm_vm_unsigned_int_arith(vm, reg1_num, reg1_bytewidth, reg2_num, reg2_bytewidth, reg3_num, reg3_bytewidth, RYVM_VM_ARITH_OP_OR); break;
      case RYVM_OP_XOR: ryvm_vm_unsigned_int_arith(vm, reg1_num, reg1_bytewidth, reg2_num, reg2_bytewidth, reg3_num, reg3_bytewidth, RYVM_VM_ARITH_OP_XOR); break;

      //note that right-hand operand will ALWAYS BE TREATED AS UNSIGNED, even if it wasnt intended
      case RYVM_OP_SHL: ryvm_vm_unsigned_int_arith(vm, reg1_num, reg1_bytewidth, reg2_num, reg2_bytewidth, reg3_num, reg3_bytewidth, RYVM_VM_ARITH_OP_SHL); break;
      case RYVM_OP_SHR: ryvm_vm_unsigned_int_arith(vm, reg1_num, reg1_bytewidth, reg2_num, reg2_bytewidth, reg3_num, reg3_bytewidth, RYVM_VM_ARITH_OP_SHR); break;

      // If bit in 3rd reg is 0, keep original bit in 2nd reg. If the bit in 3rd reg is 1, clear it to 0
      case RYVM_OP_BIC: {
        uint64_t val = vm->gen_registers[reg2_num];
        val &= ~vm->gen_registers[reg3_num];
        memcpy(&vm->gen_registers[reg1_num], &val, reg1_bytewidth);
        break;
      }


      case RYVM_OP_XORI: {
        int64_t a = ins[3];  //sign extend 8-bit immediate value
        int64_t result = vm->gen_registers[reg2_num] ^ a;

        //only copy the bytewidth specified from the result to the dest register
        uint8_t end_offset_dest = reg1_bytewidth; 
        memcpy(&vm->gen_registers[reg1_num], &result, end_offset_dest); 
        break;
      }


      /* Arithmetic For Signed/Unsigned Integers */



      case RYVM_OP_ADDI: {
        uint64_t result = 0;
        int8_t imm = ins[3];
        result = vm->gen_registers[reg2_num] + imm;
        memcpy(&vm->gen_registers[reg1_num], &result, reg1_bytewidth); 
        break;
      }

      case RYVM_OP_SUBI: {
        uint64_t result = 0;
        int8_t imm = ins[3]; //Note. Despite C forcing a implicit conversion from unsigned to signed int, due to 2's complement, the conversion results in the same bit representation.
        result = vm->gen_registers[reg2_num] - imm;
        memcpy(&vm->gen_registers[reg1_num], &result, reg1_bytewidth); 
        break;
      }


      //note to use unsigned arithmetic for addition and subtraction
      //since 2's complement makes these operations identical regardless of sign or unsigned
      case RYVM_OP_ADD: ryvm_vm_unsigned_int_arith(vm, reg1_num, reg1_bytewidth, reg2_num, reg2_bytewidth, reg3_num, reg3_bytewidth, RYVM_VM_ARITH_OP_ADD); break;
      case RYVM_OP_SUB: ryvm_vm_unsigned_int_arith(vm, reg1_num, reg1_bytewidth, reg2_num, reg2_bytewidth, reg3_num, reg3_bytewidth, RYVM_VM_ARITH_OP_SUB); break;

      case RYVM_OP_MUL: ryvm_vm_signed_int_arith(vm, reg1_num, reg1_bytewidth, reg2_num, reg2_bytewidth, reg3_num, reg3_bytewidth, RYVM_VM_ARITH_OP_MUL); break;
      case RYVM_OP_MULU: ryvm_vm_unsigned_int_arith(vm, reg1_num, reg1_bytewidth, reg2_num, reg2_bytewidth, reg3_num, reg3_bytewidth, RYVM_VM_ARITH_OP_MUL); break;
      case RYVM_OP_DIV: ryvm_vm_signed_int_arith(vm, reg1_num, reg1_bytewidth, reg2_num, reg2_bytewidth, reg3_num, reg3_bytewidth, RYVM_VM_ARITH_OP_DIV); break;
      case RYVM_OP_DIVU: ryvm_vm_unsigned_int_arith(vm, reg1_num, reg1_bytewidth, reg2_num, reg2_bytewidth, reg3_num, reg3_bytewidth, RYVM_VM_ARITH_OP_DIV); break;
      case RYVM_OP_REM: ryvm_vm_signed_int_arith(vm, reg1_num, reg1_bytewidth, reg2_num, reg2_bytewidth, reg3_num, reg3_bytewidth, RYVM_VM_ARITH_OP_REM); break;
      case RYVM_OP_REMU: ryvm_vm_unsigned_int_arith(vm, reg1_num, reg1_bytewidth, reg2_num, reg2_bytewidth, reg3_num, reg3_bytewidth, RYVM_VM_ARITH_OP_REM); break;


      /* Arithmetic For 32-bit and 64-bit floating point numbers */
      case RYVM_OP_ADDF: ryvm_vm_float_arith(vm, reg1_num, reg1_bytewidth, reg2_num, reg2_bytewidth, reg3_num, reg3_bytewidth, RYVM_VM_ARITH_OP_ADD); break;
      case RYVM_OP_SUBF: ryvm_vm_float_arith(vm, reg1_num, reg1_bytewidth, reg2_num, reg2_bytewidth, reg3_num, reg3_bytewidth, RYVM_VM_ARITH_OP_SUB); break;
      case RYVM_OP_MULF: ryvm_vm_float_arith(vm, reg1_num, reg1_bytewidth, reg2_num, reg2_bytewidth, reg3_num, reg3_bytewidth, RYVM_VM_ARITH_OP_MUL); break;
      case RYVM_OP_DIVF: ryvm_vm_float_arith(vm, reg1_num, reg1_bytewidth, reg2_num, reg2_bytewidth, reg3_num, reg3_bytewidth, RYVM_VM_ARITH_OP_DIV); break;
      case RYVM_OP_REMF: ryvm_vm_float_arith(vm, reg1_num, reg1_bytewidth, reg2_num, reg2_bytewidth, reg3_num, reg3_bytewidth, RYVM_VM_ARITH_OP_REM); break;

      /* Comparisons for signed/unsigned integers and floating point numbers */


      //compare 2 signed integers
      case RYVM_OP_CPS: {
        //note that we need to check for overflow based on the larger bytewidth of the source registers.
        int64_t res;

        //copy register values, while also sign extending them if necessary
        int64_t a = vm->gen_registers[reg2_num];
        int64_t b = vm->gen_registers[reg3_num];


        uint8_t largest_bytewidth = reg2_bytewidth > reg3_bytewidth ? reg2_bytewidth : reg3_bytewidth;
        uint8_t msb_b = *(((uint8_t*) &b) + (largest_bytewidth - 1));

        res = a - b;

        //check for signed overflow by checking if sign of result is equal to sign of 2nd operand.
        //Remember to check based on the largest bytewidth of the 2 source registers

        //get MSB of result based on largest bytewidth between 2 source registers
        uint8_t msb_r = *(((uint8_t*) &res) + (largest_bytewidth - 1));

        //set overflow flag
        ryvm_vm_flags_set_flag(vm, (msb_r & 128) == (msb_b & 128), RYVM_VM_STATUS_FLAG_V);

        //set negative bit
        ryvm_vm_flags_set_flag(vm, msb_r & 128, RYVM_VM_STATUS_FLAG_N);

        //set zero bit
        ryvm_vm_flags_set_flag(vm, res == 0, RYVM_VM_STATUS_FLAG_Z);


        //insert result in register
        memcpy(vm->gen_registers+reg1_num, &res, reg1_bytewidth);
        break;
      }

      //compare 2 unsigned integers
      case RYVM_OP_CPU: {
        //note that we need to check for overflow based on the larger bytewidth of the source registers.
        uint64_t res;

        //copy register values, and manually zero extend them by clearing most significant bytes
        uint64_t a = vm->gen_registers[reg2_num];
        uint64_t b = vm->gen_registers[reg3_num];

        uint8_t largest_bytewidth = reg2_bytewidth > reg3_bytewidth ? reg2_bytewidth : reg3_bytewidth;

        memset(((uint8_t*) &a) + (largest_bytewidth), 0, 8-largest_bytewidth);
        memset(((uint8_t*) &b) + (largest_bytewidth), 0, 8-largest_bytewidth);

        res = a - b;

        //set overflow flag
        ryvm_vm_flags_set_flag(vm, a < b, RYVM_VM_STATUS_FLAG_V);

        //set negative bit (always 0 for unsigned numbers)
        ryvm_vm_flags_set_flag(vm, 0, RYVM_VM_STATUS_FLAG_N);

        //set zero bit
        ryvm_vm_flags_set_flag(vm, res == 0, RYVM_VM_STATUS_FLAG_Z);

        //insert result in register
        memcpy(vm->gen_registers+reg1_num, &res, reg1_bytewidth);
        break;
      }

      case RYVM_OP_CPF: {
        //note that we need to check for overflow based on the larger bytewidth of the source registers.

        uint8_t overflowed = 0;

        //copy register values, and manually zero extend them by clearing most significant bytes
        uint8_t *s1 = (uint8_t*) (vm->gen_registers + reg2_num);
        uint8_t *s2 = (uint8_t*) (vm->gen_registers + reg3_num);

        uint8_t largest_bytewidth = reg2_bytewidth > reg3_bytewidth ? reg2_bytewidth : reg3_bytewidth;

        //we need to ensure that both values are doubles
        if(largest_bytewidth > 4) { 
          double a = 0; //zero out
          double b = 0;
          
          //if either value is a float, convert it to double.
          if(reg2_bytewidth <= 4) {
            float af;
            memcpy(&af, s1, 4);
            a = (double) af;
          } else {
            memcpy(&a, s1, 8);
          } 
          
          if(reg3_bytewidth <= 4) {
            float bf;
            memcpy(&bf, s2, 4);
            b = (double) bf;
          } else {
            memcpy(&b, s2, 8);
          }

          //clear exceptions since exceptions may persist across multiple floating point calculations
          feclearexcept(FE_OVERFLOW | FE_UNDERFLOW);
          double res = a - b;
          overflowed = fetestexcept(FE_OVERFLOW) | fetestexcept(FE_UNDERFLOW);

          //set negative bit (always 0 for unsigned numbers)
          ryvm_vm_flags_set_flag(vm, res < 0.0, RYVM_VM_STATUS_FLAG_N);

          //set zero bit
          ryvm_vm_flags_set_flag(vm, (res == 0.0) | (res == -0.0) , RYVM_VM_STATUS_FLAG_Z);

          //insert result in register
          memcpy(vm->gen_registers+reg1_num, &res, reg1_bytewidth);
        } 
        //TODO:If the register width is smaller than 32 bits for floating point operations, we should throw a semantic error
        //for now, we will just assume the bytewidth is 32 bits
        else {
          float a;
          float b;
          memcpy(&a, s1, 4);
          memcpy(&b, s2, 4);

          feclearexcept(FE_OVERFLOW | FE_UNDERFLOW);
          float res = a - b;
          overflowed = fetestexcept(FE_OVERFLOW) | fetestexcept(FE_UNDERFLOW);

          //set negative bit (always 0 for unsigned numbers)
          ryvm_vm_flags_set_flag(vm, res < 0.0, RYVM_VM_STATUS_FLAG_N);

          //set zero bit
          ryvm_vm_flags_set_flag(vm, (res == 0.0) | (res == -0.0) , RYVM_VM_STATUS_FLAG_Z);

          //insert result in register
          memcpy(vm->gen_registers+reg1_num, &res, reg1_bytewidth);

        }

        //set overflow flag
        ryvm_vm_flags_set_flag(vm, overflowed, RYVM_VM_STATUS_FLAG_V);
        break;
      }

      case RYVM_OP_CPSI: {
        //note that we need to check for overflow based on the larger bytewidth of the source registers.
        int64_t res;

        //copy register values, while also sign extending them if necessary
        int64_t a = vm->gen_registers[reg1_num];

        //extract 16 bits from immediate value
        int64_t b = 0;

        int16_t b16;
        uint8_t *b_bytes = (uint8_t*) &b16;
        b_bytes[0] = ins[2];
        b_bytes[1] = ins[3];

        b = b16; //sign extend immediate value to 64 bits.


        uint8_t largest_bytewidth = reg1_bytewidth > 2 ? reg1_bytewidth : 2;
        uint8_t msb_b = *(((uint8_t*) &b) + (largest_bytewidth - 1));

        res = a - b;

        //check for signed overflow by checking if sign of result is equal to sign of 2nd operand.
        //Remember to check based on the largest bytewidth of the 2 source registers

        //get MSB of result based on largest bytewidth between 2 source registers
        uint8_t msb_r = *(((uint8_t*) &res) + (largest_bytewidth - 1));

        //set overflow flag
        ryvm_vm_flags_set_flag(vm, (msb_r & 128) == (msb_b & 128), RYVM_VM_STATUS_FLAG_V);

        //set negative bit
        ryvm_vm_flags_set_flag(vm, msb_r & 128, RYVM_VM_STATUS_FLAG_N);

        //set zero bit
        ryvm_vm_flags_set_flag(vm, res == 0, RYVM_VM_STATUS_FLAG_Z);

        break;
      }

      case RYVM_OP_CPUI: {
        //note that we need to check for overflow based on the larger bytewidth of the source registers.
        uint64_t res;

        //copy register values, and manually zero extend them by clearing most significant bytes
        uint64_t a = vm->gen_registers[reg1_num];

        //extract 16 bits from immediate value, zero extending it.
        uint64_t b = 0;

        uint8_t *b_bytes = (uint8_t*) &b;
        b_bytes[0] = ins[2];
        b_bytes[1] = ins[3];

        uint8_t largest_bytewidth = reg2_bytewidth > reg3_bytewidth ? reg2_bytewidth : reg3_bytewidth;

        memset(((uint8_t*) &a) + (largest_bytewidth), 0, 8-largest_bytewidth);
        memset(((uint8_t*) &b) + (largest_bytewidth), 0, 8-largest_bytewidth);

        res = a - b;

        //set overflow flag
        ryvm_vm_flags_set_flag(vm, a < b, RYVM_VM_STATUS_FLAG_V);

        //set negative bit (always 0 for unsigned numbers)
        ryvm_vm_flags_set_flag(vm, 0, RYVM_VM_STATUS_FLAG_N);

        //set zero bit
        ryvm_vm_flags_set_flag(vm, res == 0, RYVM_VM_STATUS_FLAG_Z);

        break;
      }


      /* Jumps */

      case RYVM_OP_B: {
        int32_t offset = ryvm_vm_helper_cast_int_24_to_32(ins+1);
        ryvm_vm_pc_set(vm, ryvm_vm_pc(vm) + offset);
        break;

      }

      

      /*
      BEQ #off        ; (Z=1)
      BNE #off        ; (Z=0)
      BLT #off        ; (N!=V); if less than , jump to PC-relative offset
      BGT #off        ; (N=V and Z=0); if greater than, jump to PC-relative offset
      BLE #off        ; (N!=V or Z=1); if less or equal to, jump to PC-relative offset
      BGE #off        ; (N=V or Z=1); if greater or equal to, jump to PC-relative offset

      */

      case RYVM_OP_BEQ: {
        // if zero bit is not set, it is not equal
        if((ryvm_vm_flags(vm) & RYVM_VM_STATUS_FLAG_Z) == 0) {
          break;
        }
        int32_t offset = ryvm_vm_helper_cast_int_24_to_32(ins+1);
        ryvm_vm_pc_set(vm, ryvm_vm_pc(vm) + offset);
        break;
      }

      case RYVM_OP_BNE: {
        // if zero bit is set, it is equal
        if((ryvm_vm_flags(vm) & RYVM_VM_STATUS_FLAG_Z) != 0) {
          break;
        }

        int32_t offset = ryvm_vm_helper_cast_int_24_to_32(ins+1);
        ryvm_vm_pc_set(vm, ryvm_vm_pc(vm) + offset);
        break;
      }


      case RYVM_OP_BLT: {
        // N!=V
        uint64_t sf = ryvm_vm_flags(vm);
        //if both flags are equal, dont branch
        if((sf & RYVM_VM_STATUS_FLAG_N) == (sf & RYVM_VM_STATUS_FLAG_V)) {
          break;
        }

        int32_t offset = ryvm_vm_helper_cast_int_24_to_32(ins+1);

        ryvm_vm_pc_set(vm, ryvm_vm_pc(vm) + offset);
        break;
      }

      case RYVM_OP_BGT: {
        // N=V and Z=0
        uint64_t sf = ryvm_vm_flags(vm);
        //if N!=V or Z=1, dont branch
        if((sf & RYVM_VM_STATUS_FLAG_N) != (sf & RYVM_VM_STATUS_FLAG_V) || sf & RYVM_VM_STATUS_FLAG_Z) {
          break;
        }

        int32_t offset = ryvm_vm_helper_cast_int_24_to_32(ins+1);
        ryvm_vm_pc_set(vm, ryvm_vm_pc(vm) + offset);
        break;
      }

      case RYVM_OP_BLE: {
        // N!=V or Z=1
        uint64_t sf = ryvm_vm_flags(vm);
        //if N=V and Z=0, dont branch
        if((sf & RYVM_VM_STATUS_FLAG_N) == (sf & RYVM_VM_STATUS_FLAG_V) && ((sf & RYVM_VM_STATUS_FLAG_Z) == 0)) {
          break;
        }

        int32_t offset = ryvm_vm_helper_cast_int_24_to_32(ins+1);
        ryvm_vm_pc_set(vm, ryvm_vm_pc(vm) + offset);
        break;
      }

      case RYVM_OP_BGE: {
        // N=V or Z=1
        uint64_t sf = ryvm_vm_flags(vm);
        //if N!=V and Z=0, dont branch
        if((sf & RYVM_VM_STATUS_FLAG_N) != (sf & RYVM_VM_STATUS_FLAG_V) && ((sf & RYVM_VM_STATUS_FLAG_Z) == 0)) {
          break;
        }

        int32_t offset = ryvm_vm_helper_cast_int_24_to_32(ins+1);
        ryvm_vm_pc_set(vm, ryvm_vm_pc(vm) + offset);
        break;
      }

      case RYVM_OP_BL: {
        //set LR to PC of next instruction
        vm->gen_registers[reg1_num] = ryvm_vm_pc(vm);

        //16-bit offset 
        //apparently this fails to work
        // int16_t *off = ins + 2;
        
        //but this does work.
        int16_t offset;
        uint8_t *offset_bytes = (uint8_t*) &offset;
        offset_bytes[0] = ins[2];
        offset_bytes[1] = ins[3];
        ryvm_vm_pc_set(vm, ryvm_vm_pc(vm) + offset);
        break;


      }

      case RYVM_OP_BR: {
        //16-bit offset 
        //apparently this fails to work
        // int16_t *off = ins + 2;
        
        //but this does work.
        int16_t offset;
        uint8_t *offset_bytes = (uint8_t*) &offset;
        offset_bytes[0] = ins[2];
        offset_bytes[1] = ins[3];

        ryvm_vm_pc_set(vm, vm->gen_registers[reg1_num] + offset);
        break;
      }

      /* Stack Related Stuff */
      case RYVM_OP_BLR: {
        int8_t offset = ins[3];

        vm->gen_registers[reg1_num] = ryvm_vm_pc(vm);

        ryvm_vm_pc_set(vm, vm->gen_registers[reg2_num] + offset);
        break;
      }


      /* Misc */

      //similar to the x86-64 Linux calling convention, the 0th register is the syscall number, and any values returned
      //from the syscall are stored at the 0th register
      case RYVM_OP_SYS: {
        uint32_t offset;

        //set individual bytes of 24 bit number
        uint8_t *offset_bytes = (uint8_t*) &offset;
        offset_bytes[0] = ins[1];
        offset_bytes[1] = ins[2];
        offset_bytes[2] = ins[3];
        offset_bytes[3] = 0;

        switch(offset) {
          //kill vm
          case 0:
            vm->is_running = 0;
            result = vm->gen_registers[0];
            break;
          //print single register from W1
          case 1: 
            printf("%lld\n", vm->gen_registers[1]);
            break;
          case 2: {
            double *f = (double*) &vm->gen_registers[1];
            printf("%lf\n", *f);
            break;
          }
          case 3:
            printf("%s\n", (char*) vm->gen_registers[1]);
            break;
          case 4: {
            float *f = (float*) &vm->gen_registers[1];
            printf("%f\n", *f);
            break;
          }
          default:
            goto syscall_fail;
        }

       
        //syscall_good:
          break;
        syscall_fail:
          vm->is_running = 0;
          printf("ERROR: Invalid syscall value!\n");
          continue;

      }
      default:
        assert(0);
    }


  }



  return result;
}


void ryvm_vm_free(struct ryvm *vm) {
  free(vm->stack);
  free(vm->data_and_code);
}
