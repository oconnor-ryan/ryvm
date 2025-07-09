#ifndef RYVM_LEXER_H
#define RYVM_LEXER_H

#include <stdint.h>
#include "opcodes.h"
#include "memory/string_builder.h"
#include "types.h"

//#include "assembler.h"

#include <stdio.h>




enum ryvm_token_tag {
  RYVM_TOKEN_SECTION_MAX_STACK_SIZE,
  RYVM_TOKEN_SECTION_DATA,
  RYVM_TOKEN_SECTION_DATA_BYTE,
  RYVM_TOKEN_SECTION_DATA_DOUBLE,
  RYVM_TOKEN_SECTION_DATA_QUAD,
  RYVM_TOKEN_SECTION_DATA_OCT,
  RYVM_TOKEN_SECTION_DATA_ASCIZ,
  RYVM_TOKEN_SECTION_DATA_FLOAT16_HEADER,
  RYVM_TOKEN_SECTION_DATA_FLOAT32_HEADER,
  RYVM_TOKEN_SECTION_DATA_FLOAT64_HEADER,

  RYVM_TOKEN_SECTION_TEXT,

  RYVM_TOKEN_LF,
  RYVM_TOKEN_EOF,

  RYVM_TOKEN_STRING_LITERAL,
  RYVM_TOKEN_INT_LITERAL,
  RYVM_TOKEN_FLOAT_LITERAL,
  RYVM_TOKEN_REGISTER,
  RYVM_TOKEN_LABEL,
  RYVM_TOKEN_LABEL_ADR_OF_EXPR,
  RYVM_TOKEN_LABEL_PC_OFF_EXPR,
  RYVM_TOKEN_OPCODE,

};

struct ryvm_token {
  enum ryvm_token_tag tag;
  union {
    uint32_t data_size;
    char *label_name; //used for both ADR_OF and PC_OFF label expressions
    enum ryvm_opcode opcode;

    union num num;
    char *string_lit;

    uint8_t reg; //1st 2 bits are the access type (00 for 1byte, 01 for 2byte, 10 for 4byte, 11 for 8byte)
  } d;
};






struct ryvm_lexer {
  uint64_t source_row;
  uint64_t source_col;

  uint8_t reached_end_line;
  uint8_t reached_eof;

  //we use this to allow 1 token of lookahead for our grammer.
  struct ryvm_token unparsed_token;
  uint8_t has_unparsed_token;

  int lexer_failed;
  struct memory_array_builder temp_word;

  struct memory_allocator *mem;

  FILE *src;

};


int ryvm_lexer_init(struct ryvm_lexer *lexer, FILE *src);
void ryvm_lexer_free(struct ryvm_lexer *lexer);

struct ryvm_token ryvm_lexer_get_token(struct ryvm_lexer *lexer);



#endif// RYVM_LEXER_H
