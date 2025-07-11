#include "assembler.h"
#include "vm.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include "helper.h"

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

inline void ryvm_vm_byte_to_reg(uint8_t reg, uint8_t *reg_bytewidth, uint8_t *reg_num) {
  //registers are little endian, so they look like this in memory
  /*
        LSB                        MSB
    B |--------| 
    D |--------|--------|
    Q |--------|--------|--------|--------|
    O |--------|--------|--------|--------|--------|--------|--------|--------|

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
inline void ryvm_vm_pc_inc(struct ryvm *vm)        {vm->gen_registers[RYVM_PC_REG] += 4;} //size of instruction is 4 bytes


inline void ryvm_vm_frame_ptr_set(struct ryvm *vm, uint64_t new_val)  {vm->gen_registers[RYVM_FP_REG] = new_val;}
inline void ryvm_vm_stack_ptr_set(struct ryvm *vm, uint64_t new_val)  {vm->gen_registers[RYVM_SP_REG] = new_val;}
inline void ryvm_vm_flags_set(struct ryvm *vm, uint64_t new_val)      {vm->gen_registers[RYVM_SF_REG] = new_val;}
inline void ryvm_vm_pc_set(struct ryvm *vm, uint64_t new_val)         {vm->gen_registers[RYVM_PC_REG] = new_val;}
inline void ryvm_vm_lr_set(struct ryvm *vm, uint64_t new_val)         {vm->gen_registers[RYVM_LR_REG] = new_val;}


inline uint8_t ryvm_vm_flag_bool(struct ryvm *vm)   {return ryvm_vm_flags(vm) & 1;}
inline void ryvm_vm_flag_bool_set(struct ryvm *vm, uint8_t bool_val)  { 
  //zero out leading MSB to avoid messing with other flags
  bool_val &= 1;
  ryvm_vm_flags_set(vm, ryvm_vm_flags(vm) | bool_val);
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


    //Because most arithmetic and comparison operations are programmed almost exactly the same
    //with the exception of the operator character used, we will use macros


    //we dont have to worry about the register bytewidth for the source, 
    //since the intermediate value does not modify any registers
    #define RYVM_MACRO_ARITH_BIN_OP(op, type) { \
      type a = 0; \
      type b = 0; \
      /* Due to an issue where casting can change the underlying bits, we use memcpy instead*/ \
      /* This also allows us to copy specific bytes from a register to conform to the requested bytewidth of the registers*/ \
      memcpy(&a, &vm->gen_registers[reg2_num], reg2_bytewidth); \
      memcpy(&b, &vm->gen_registers[reg3_num], reg3_bytewidth); \
      type value = a op b; \
      uint8_t end_offset_dest = reg1_bytewidth; \
      memcpy(&vm->gen_registers[reg1_num], &value, end_offset_dest); \
      break; \
    }

    

    //floating points require special care since you cannot "sign-extend" them by appending 0's to the bit representation.
    //casting from float to double and vise versa is more complex, so we'll let C do it for us using explicit types
    //and casting.
    #define RYVM_MACRO_ARITH_FLOAT_BIN_OP_BODY(op, type1, type2) { \
      type1 a = 0; \
      type2 b = 0; \
      memcpy(&a, &vm->gen_registers[reg2_num], reg2_bytewidth); \
      memcpy(&b, &vm->gen_registers[reg3_num], reg3_bytewidth); \
      /* Depending on the bytewidth of the dest register, we need to perform a single or double precision floating point operation*/ \
      /* We must do this because you cannot easily "sign-extend" a floating point number like you can with 2's complement integers*/ \
      if(reg1_bytewidth > 4) { \
        double value = (double) a op (double) b; \
        uint8_t end_offset_dest = reg1_bytewidth; \
        memcpy(&vm->gen_registers[reg1_num], &value, end_offset_dest); \
      } else { \
        float value = (float) a op (float) b; \
        uint8_t end_offset_dest = reg1_bytewidth; \
        memcpy(&vm->gen_registers[reg1_num], &value, end_offset_dest); \
      } \
    }

    //we do need to worry about about bytewidth of source registers for floating point
    //since the format of the floating point number makes it much harder only add certain bytes
    #define RYVM_MACRO_ARITH_FLOAT_BIN_OP(op) { \
      if(reg2_bytewidth > 4 && reg3_bytewidth > 4) \
        RYVM_MACRO_ARITH_FLOAT_BIN_OP_BODY(op, double, double) \
      else if(reg2_bytewidth > 4 && reg3_bytewidth <= 4) \
        RYVM_MACRO_ARITH_FLOAT_BIN_OP_BODY(op, double, float) \
      else if(reg2_bytewidth <= 4 && reg3_bytewidth > 4) \
        RYVM_MACRO_ARITH_FLOAT_BIN_OP_BODY(op, float, double) \
      else \
        RYVM_MACRO_ARITH_FLOAT_BIN_OP_BODY(op, float, float) \
      break; \
    }
    
    //macro for comparision operators of integers
    #define RYVM_MACRO_COMP_BIN_OP(op, type) { \
      type a = 0; \
      type b = 0; \
      memcpy(&a, &vm->gen_registers[reg2_num], reg2_bytewidth); \
      memcpy(&b, &vm->gen_registers[reg3_num], reg3_bytewidth); \
      /* Depending on the bytewidth of the dest register, we need to perform a single or double precision floating point operation*/ \
      /* We must do this because you cannot easily "sign-extend" a floating point number like you can with 2's complement integers*/ \
      uint64_t value = a op b; \
      uint8_t end_offset_dest = reg1_bytewidth; \
      memcpy(&vm->gen_registers[reg1_num], &value, end_offset_dest); \
      break; \
    }

    //floating point comparisons
    #define RYVM_MACRO_COMP_FLOAT_BIN_OP_BODY(op, type1, type2) { \
      type1 a = 0; \
      type2 b = 0; \
      memcpy(&a, &vm->gen_registers[reg2_num], reg2_bytewidth); \
      memcpy(&b, &vm->gen_registers[reg3_num], reg3_bytewidth); \
      /* Depending on the bytewidth of the dest register, we need to perform a single or double precision floating point operation*/ \
      /* We must do this because you cannot easily "sign-extend" a floating point number like you can with 2's complement integers*/ \
      uint64_t value = a op b; \
      uint8_t end_offset_dest = reg1_bytewidth; \
      memcpy(&vm->gen_registers[reg1_num], &value, end_offset_dest); \
      break; \
    }

    //floating point comparison while accounting for bytewidths of registers.
    #define RYVM_MACRO_COMP_FLOAT_BIN_OP(op) { \
      if(reg1_bytewidth > 4 && reg2_bytewidth > 4) \
        RYVM_MACRO_COMP_FLOAT_BIN_OP_BODY(op, double, double) \
      else if(reg1_bytewidth > 4 && reg2_bytewidth <= 4) \
        RYVM_MACRO_COMP_FLOAT_BIN_OP_BODY(op, double, float) \
      else if(reg1_bytewidth <= 4 && reg2_bytewidth > 4) \
        RYVM_MACRO_COMP_FLOAT_BIN_OP_BODY(op, float, double) \
      else \
        RYVM_MACRO_COMP_FLOAT_BIN_OP_BODY(op, float, float) \
      break; \
    }
      


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
        uint64_t value = *((uint64_t*) (vm->gen_registers[reg2_num] + offset));

        //you may choose the bytewidth of the destination register, but 
        //the src register's bytewidth does not matter. The src register will
        //always be read in its entirety
        uint8_t end_offset_dest = reg1_bytewidth;
        memcpy(dest, &value, end_offset_dest);

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
      case RYVM_OP_AND: RYVM_MACRO_ARITH_BIN_OP(&, uint64_t)
      case RYVM_OP_OR: RYVM_MACRO_ARITH_BIN_OP(|, uint64_t)
      case RYVM_OP_XOR: RYVM_MACRO_ARITH_BIN_OP(^, uint64_t)

      //note that right-hand operand will ALWAYS BE TREATED AS UNSIGNED, even if it wasnt intended
      case RYVM_OP_SHL: RYVM_MACRO_ARITH_BIN_OP(<<, uint64_t)
      case RYVM_OP_SHR: RYVM_MACRO_ARITH_BIN_OP(>>, uint64_t)

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


      //use uint64_t for unsigned arithmetic and int64_t for signed arithmetic.
      //We use uint64_t and int64_t instead of smaller sizes so that we 
      //can sign-extend all integers to 64 bits before performing arithmetic on them. 
      case RYVM_OP_ADD: RYVM_MACRO_ARITH_BIN_OP(+, uint64_t)
      case RYVM_OP_SUB: RYVM_MACRO_ARITH_BIN_OP(-, uint64_t)
      case RYVM_OP_MUL: RYVM_MACRO_ARITH_BIN_OP(*, uint64_t)
      case RYVM_OP_MULU: RYVM_MACRO_ARITH_BIN_OP(*, int64_t)
      case RYVM_OP_DIV: RYVM_MACRO_ARITH_BIN_OP(/, uint64_t)
      case RYVM_OP_DIVU: RYVM_MACRO_ARITH_BIN_OP(/, int64_t)
      case RYVM_OP_REM: RYVM_MACRO_ARITH_BIN_OP(%, uint64_t)
      case RYVM_OP_REMU: RYVM_MACRO_ARITH_BIN_OP(%, int64_t)


      /* Arithmetic For 32-bit and 64-bit floating point numbers */
      case RYVM_OP_ADDF: RYVM_MACRO_ARITH_FLOAT_BIN_OP(+)
      case RYVM_OP_SUBF: RYVM_MACRO_ARITH_FLOAT_BIN_OP(-)
      case RYVM_OP_MULF: RYVM_MACRO_ARITH_FLOAT_BIN_OP(*)
      case RYVM_OP_DIVF: RYVM_MACRO_ARITH_FLOAT_BIN_OP(/)

      //there is no builtin C operator to perform modulus operation on floating point, 
      //so we will need to use fmod() for doubles and fmodf for floats
      case RYVM_OP_REMF: {
        if(reg2_bytewidth > 4 && reg3_bytewidth > 4) {
          double a = 0; 
          double b = 0; 
          memcpy(&a, &vm->gen_registers[reg2_num], reg2_bytewidth); 
          memcpy(&b, &vm->gen_registers[reg3_num], reg3_bytewidth); 
          double value = fmod(a, b);
          uint8_t end_offset_dest = reg1_bytewidth; 
          memcpy(&vm->gen_registers[reg1_num], &value, end_offset_dest); 
        }
        //note that if 1 register is a float32 while the other is a float64, the float32 gets 
        //promoted to a float64
        else if(reg2_bytewidth > 4 && reg3_bytewidth <= 4) {
          double a = 0; 
          float b = 0; 
          memcpy(&a, &vm->gen_registers[reg2_num], reg2_bytewidth); 
          memcpy(&b, &vm->gen_registers[reg3_num], reg3_bytewidth); 
          double value = fmod(a, (double) b);
          uint8_t end_offset_dest = reg1_bytewidth; 
          memcpy(&vm->gen_registers[reg1_num], &value, end_offset_dest); 
        } else if(reg2_bytewidth <= 4 && reg3_bytewidth > 4) {
          float a = 0; 
          double b = 0; 
          memcpy(&a, &vm->gen_registers[reg2_num], reg2_bytewidth); 
          memcpy(&b, &vm->gen_registers[reg3_num], reg3_bytewidth); 
          double value = fmod((double) a, b);
          uint8_t end_offset_dest = reg1_bytewidth; 
          memcpy(&vm->gen_registers[reg1_num], &value, end_offset_dest); 
        } else {
          float a = 0; 
          float b = 0; 
          memcpy(&a, &vm->gen_registers[reg2_num], reg2_bytewidth); 
          memcpy(&b, &vm->gen_registers[reg3_num], reg3_bytewidth); 
          float value = fmodf(a, b);
          uint8_t end_offset_dest = reg1_bytewidth; 
          memcpy(&vm->gen_registers[reg1_num], &value, end_offset_dest); 
        }
        break; 
      }

      /* Conditionals for signed/unsigned integers */

      //remember, all integers are sign-extended (or zero-extended for unsigned integers) to 64 bits
      //for less work for the interpreter.


      //note that EQ and NE will work for both floating point and integers, since we simply just check the bit representations
      case RYVM_OP_EQ: RYVM_MACRO_COMP_BIN_OP(==, uint64_t)
      case RYVM_OP_NE: RYVM_MACRO_COMP_BIN_OP(!=, uint64_t)

      case RYVM_OP_LTS: RYVM_MACRO_COMP_BIN_OP(<, int64_t)
      case RYVM_OP_LTU: RYVM_MACRO_COMP_BIN_OP(<, uint64_t)
      case RYVM_OP_GTS: RYVM_MACRO_COMP_BIN_OP(>, int64_t)
      case RYVM_OP_GTU: RYVM_MACRO_COMP_BIN_OP(>, uint64_t)

      case RYVM_OP_LES: RYVM_MACRO_COMP_BIN_OP(<=, int64_t)
      case RYVM_OP_LEU: RYVM_MACRO_COMP_BIN_OP(<=, uint64_t)
      case RYVM_OP_GES: RYVM_MACRO_COMP_BIN_OP(>=, int64_t)
      case RYVM_OP_GEU: RYVM_MACRO_COMP_BIN_OP(>=, uint64_t)


      /* Conditionals for floating points (note that EQ and NE both check equality of integers and floating point numbers) */


      //TODO: Add RyVM Opcodes to convert a floating point number to an integer and vise versa.
      // STF O0 O1 O2   ; Signed integer to float
      // UTF O0 O1 O2   ; Unsigned integer to float
      // FTS O0 O1 O2   ; Float to signed integer
      // FTU O0 O1 O2   ; Float to unsigned integer

      case RYVM_OP_LTF: RYVM_MACRO_COMP_FLOAT_BIN_OP(<)
      case RYVM_OP_GTF: RYVM_MACRO_COMP_FLOAT_BIN_OP(>)
      case RYVM_OP_LEF: RYVM_MACRO_COMP_FLOAT_BIN_OP(<=)
      case RYVM_OP_GEF: RYVM_MACRO_COMP_FLOAT_BIN_OP(>=)


      /* Jumps */

      case RYVM_OP_B: {
        int32_t offset;

        //set individual bytes of 24 bit number
        uint8_t *offset_bytes = (uint8_t*) &offset;
        offset_bytes[0] = ins[1];
        offset_bytes[1] = ins[2];
        offset_bytes[2] = ins[3];
        offset_bytes[3] = 0;


        ryvm_vm_pc_set(vm, ryvm_vm_pc(vm) + offset);
        break;

      }
      case RYVM_OP_BZ: {
        if(!vm->gen_registers[reg1_num]) {

          //apparently this fails to work
          // int16_t *off = ins + 2;
          
          //but this does work.
          int16_t offset;
          uint8_t *offset_bytes = (uint8_t*) &offset;
          offset_bytes[0] = ins[2];
          offset_bytes[1] = ins[3];

          //I hate implicit conversions.

          ryvm_vm_pc_set(vm, ryvm_vm_pc(vm) + offset);
        }
        break; 
      }
      case RYVM_OP_BNZ: {
        if(vm->gen_registers[reg1_num]) {
          //apparently this fails to work
          // int16_t *off = ins + 2;
          
          //but this does work.
          int16_t offset;
          uint8_t *offset_bytes = (uint8_t*) &offset;
          offset_bytes[0] = ins[2];
          offset_bytes[1] = ins[3];

          //I hate implicit conversions.
          ryvm_vm_pc_set(vm, ryvm_vm_pc(vm) + offset);
        }
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
