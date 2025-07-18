#ifndef RYVM_ASSEMBLER_H
#define RYVM_ASSEMBLER_H

#include <stdio.h>

#include "../opcodes.h"
#include "lexer.h"


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


extern const char *RYVM_DATA_ASCIZ_HEADER;




enum ryvm_assembler_mode {
  RYVM_ASSEMBLER_MODE_CONFIG,  //reads basic configuration of the VM (i.e. max stack size)
  RYVM_ASSEMBLER_MODE_DATA,   //read modifiable data (can also store .buffer globals)
  RYVM_ASSEMBLER_MODE_TEXT,    //read instructions
};




//change this order and you will die
enum ryvm_assembler_data_entry_type {
  RYVM_ASSEMBLER_DATA_ENTRY_TYPE_1BYTE,
  RYVM_ASSEMBLER_DATA_ENTRY_TYPE_2BYTE,
  RYVM_ASSEMBLER_DATA_ENTRY_TYPE_4BYTE,
  RYVM_ASSEMBLER_DATA_ENTRY_TYPE_8BYTE,

  RYVM_ASSEMBLER_DATA_ENTRY_TYPE_ASCII_Z, //null terminated 8bit ascii string

  RYVM_ASSEMBLER_DATA_ENTRY_TYPE_SPACE //represents a variable number of uninitialized bytes.
};


struct ryvm_assembler_label {
  char *label;
  uint64_t relative_address; //the actual value
  uint8_t has_address;
};

struct ryvm_assembler_reloc_entry {
  uint64_t hole_relative_address;
  uint64_t relative_address_value;
};



struct ryvm_assembler_ins {
  uint8_t opcode;
  uint8_t regs[3];
  uint8_t has_placeholder;
  struct ryvm_token placeholder; //can be null. Each instruction may have only 1 placeholder (label expression, pc-offset expression, or Int/Float literal)
};


struct ryvm_assembler_data_entry {
  enum ryvm_assembler_data_entry_type tag;
  uint8_t using_placeholder; //some instructions require either a immediate or a placeholder, so we need this boolean to track this.
  union {
    union num num;
    char *ascii;
    struct ryvm_token placeholder; //FIXME instead of storing placeholder directly, we can store an index to the relocation table entry.
  } d;
};

struct  ryvm_assembler_text_entry {
  uint8_t is_ins; //if not instruction, it is a raw data entry
  union {
    struct ryvm_assembler_ins ins;
    struct ryvm_assembler_data_entry data;
  } d;
  
};




struct ryvm_assembler_config {
  uint64_t max_stack_size; //default is 1 MB
};


struct ryvm_assembler_state {
  enum ryvm_assembler_mode mode;

  struct ryvm_lexer lex;

  struct ryvm_assembler_config config;


  //our "symbol table" for labels.
  // type: struct ryvm_assembler_label
  struct memory_array_builder labels; 

  //struct ryvm_assembler_data_pool_entry
  struct memory_array_builder data;

  //stores instructions and pools
  //struct ryvm_assembler_text_entry
  struct memory_array_builder text;

  //struct ryvm_assembler_reloc_entry
  struct memory_array_builder reloc_entries;




  uint64_t current_relative_address;

  uint64_t relative_address_text_section;
  uint64_t sizeof_text_section;



  uint8_t failed;

  FILE *input;
  FILE *output;

};

int ryvm_assemble_to_bytecode(FILE *in, FILE *out);


#endif// RYVM_ASSEMBLER_H

