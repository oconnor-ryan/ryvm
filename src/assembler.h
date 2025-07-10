#ifndef RYVM_ASSEMBLER_H
#define RYVM_ASSEMBLER_H


#include "opcodes.h"
#include "lexer.h"
#include <stdio.h>


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



/*
  Grammer:

  Prog ::= Config LF Data LF Text

  Config ::= MaxStackSize Integer


  Data ::= '.data' LF DataBody
  DataBody ::=  DataEntry ("" | LF DataBody)
  DataEntry ::= DataRawByteEntry | AsciiEntry
  AsciiEntry ::= '.asciz' WS '"' AsciiList '"'
  DataRawByteEntry ::= ('.eword' | '.qword' | '.hword' | '.word') WS Integer
  DataFloatEntry ::= ('.qwordf' | '.hwordf' | '.wordf') WS Float
  DataBufferEntry ::= ('.buffer' ) WS Integer



  Text ::= '.text' LF TextBody
  TextBody ::= (Instruction | '.pool') LF ("" | TextBody)  ; .pool tells assembler to insert literal pool here instead of end of section

  Instruction ::= Opcode WS ("" | InstructionArg) WS ("" | InstructionArg) WS ("" | InstructionArg)
  InstructionArg ::= Register | Integer

  Opcode ::= <all opcodes listed in opcodes.h>
  Register ::= ('E' | 'Q' | 'H' | 'W') Digit ("" | Digit)

  AsciiList ::= AsciiChar ("" | AsciiList)
  AsciiChar ::= <Set of all ASCII chars> 

  WS ::= ' ' ("" | WS)
  Float ::= Integer '.' Integer
  Integer ::= Digit ("" | Integer)
  Digit ::= '0' | '1' | '2' | '3' | '4' | '5' | '6' | '7' | '8' | '9'
  LF ::= '\n'

*/

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

  RYVM_ASSEMBLER_DATA_ENTRY_TYPE_2BYTE_FLOAT,
  RYVM_ASSEMBLER_DATA_ENTRY_TYPE_4BYTE_FLOAT,
  RYVM_ASSEMBLER_DATA_ENTRY_TYPE_8BYTE_FLOAT,

  RYVM_ASSEMBLER_DATA_ENTRY_TYPE_ASCII_Z, //null terminated 8bit ascii string

  RYVM_ASSEMBLER_DATA_ENTRY_TYPE_SPACE //represents a variable number of uninitialized bytes.
};


struct ryvm_assembler_label {
  char *label;
  uint64_t relative_address; //the actual value
  uint8_t has_address;

  //if the relative address is out of range, these values will be set so that
  //the label's address can be fetched from a nearby pool instead
  uint64_t nearest_pool_index;
  uint64_t nearest_pool_address;
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



  struct memory_array_builder temp;
  struct memory_allocator *mem;

  uint64_t current_relative_address;

  uint64_t relative_address_text_section;
  uint64_t sizeof_text_section;



  uint8_t failed;

  FILE *input;
  FILE *output;

};

int ryvm_assemble_to_bytecode(FILE *in, FILE *out);


#endif// RYVM_ASSEMBLER_H

