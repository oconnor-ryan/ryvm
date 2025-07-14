#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "lexer.h"
#include "assembler.h"
#include "../helper.h"

char ryvm_lexer_read_char(struct ryvm_lexer *lexer) {
  lexer->reached_end_line = 0;
  lexer->reached_eof = 0;

  char c = fgetc(lexer->src);
  if(c == '\n') {
    lexer->source_col = 1;
    lexer->source_row++;
    lexer->reached_end_line = 1;

    return c;
  }
  if(c == EOF) {
    lexer->reached_eof = 1;
    return c;
  }
  lexer->source_col++;
  return c;
}

//read from semicolon to LF character or EOF character
char ryvm_lexer_consume_comment(struct ryvm_lexer *lexer) {
  char c;
  while((c = ryvm_lexer_read_char(lexer)) != EOF && c != '\n');
  return c;
}

//keep consuming chars until a non-whitespace character (excluding \n) is found or EOF is reached.
char ryvm_lexer_read_first_nonspace_char(struct ryvm_lexer *lexer) {
  char c;
  while((c = ryvm_lexer_read_char(lexer)) != EOF && c != '\n' && isspace(c));
  if(c == ';') {
    return ryvm_lexer_consume_comment(lexer);
  }

  //unset this value to avoid skiping lines
  if(c == '\n') {
    lexer->reached_end_line = 0;
  }
  return c;
}






int ryvm_lexer_read_word(struct ryvm_lexer *lexer, char init_char) {
  memory_string_builder_reset(&lexer->temp_word);

  //add 1st character
  if(!memory_string_builder_append_char(&lexer->temp_word, init_char)) {
    return 0;
  }

  char c;


  while((c = ryvm_lexer_read_char(lexer)) != EOF && !isspace(c)) {
    if(c == ';') {
      ryvm_lexer_consume_comment(lexer);
      return 1;
    }
    if(!memory_string_builder_append_char(&lexer->temp_word, c)) {
      return 0;
    }
  }

  return 1;
}


//returns 0 for no number, 1 for integer, and 2 for float
int ryvm_lexer_parse_as_number(char *num_str, uint32_t num_str_len, union num *data, uint8_t is_neg) {
  if(!isdigit(num_str[0])) {
    return 0;
  }
  int is_float = 0;

  for(uint32_t i = 1; i < num_str_len; i++) {
    if(!isdigit(num_str[i])) {
      if(num_str[i] == '.') {

        //We already encountered 1 '.', so this is not a properly formated real number
        if(is_float) {
          return 0;
        }

        is_float = 1;
        continue;
      }

      //invalid number
      return 0;
    }
  }

  if(is_float) {
    data->f64 = atof(num_str);
    if(is_neg) {
      data->f64 = -data->f64;
    }
    return 2;
  } else {
    char *endptr;
    data->u64 = strtoull(num_str, &endptr, 10);

    if(*endptr != '\0') {
      return 0; //not a true integer.
    }
    
    if(is_neg) {
      data->s64 = -data->u64;
    }

    return 1;
  }
}

int ryvm_lexer_read_string(struct ryvm_lexer *lexer) {
  char c;

  //first, let's check the initial word that was parsed and search for a " 
  //if set to true, the next character after the escaping char is marked as escaped.
  //once the escaped char is added to the string, it is set back to false.
  uint8_t escape_next_char = 0;
  
  while(1) {
    c = ryvm_lexer_read_char(lexer);

    //if string is not closed
    if(c == EOF) {
      //tokenizer_error_set(state->error, TOKENIZER_ERROR_NO_CLOSING_QUOTE, state->line_row, state->line_col, "No closing quote %s found for the quote at line %d, column %d!", quote_start_line, quote_start_col);
      lexer->lexer_failed = 1;
      return 0;
    }


    //end of quote. Only stop if this quote is not being escaped
    if(c == '"' && !escape_next_char) {
      break;
    }

    if(!memory_string_builder_append_char(&lexer->temp_word, c)) {
      //tokenizer_error_set(state->error, TOKENIZER_ERROR_MEM_ALLOC_FAIL, state->line_row, state->line_col, "Cannot allocate enough memory to store token!");
      lexer->lexer_failed = 1;
      return 0;
    }

    //if escape next char is already set, set to false
    //otherwise, check if we found the escape char
    escape_next_char = escape_next_char ? 0 : c == '\\';
  }

  //if string is closed
  return 1;

}

int ryvm_lexer_init(struct ryvm_lexer *lexer, FILE *src) {
  lexer->source_row = 1;
  lexer->source_col = 1;
  lexer->src = src;
  lexer->lexer_failed = 0;
  lexer->reached_end_line = 0;
  lexer->reached_eof = 0;
  lexer->has_unparsed_token = 0;


  /* WARNING, Do NOT USE MEMORY_ALLOCATOR_REGION_REALLOC*/
  // Remember that reallocating the memory block does not update the pointers that point
  //to entries in that block.
  //This is why we had undefined behavior before, After reallocation, all of our char pointers for labels 
  //pointed to memory that was freed after it was moved to a new block.
  lexer->mem = memory_create(500, MEMORY_ALLOCATOR_REGION_LINKED_LIST);
  if(lexer->mem == NULL) {
    return 0;
  }

  //using realloc here is fine since we only grab a pointer from this memory block
  //after the string builder is done.
  if(!memory_string_builder_init(&lexer->temp_word, 20, MEMORY_ALLOCATOR_REGION_REALLOC)) {
    memory_free(lexer->mem);
    return 0;
  }
  
  //using realloc here is fine since we only grab a pointer from this memory block
  //after the string builder is done.
  lexer->temp_mem = memory_create(20, MEMORY_ALLOCATOR_REGION_REALLOC);
  if(lexer->temp_mem == NULL) {
    memory_free(lexer->mem);
    memory_array_builder_free(&lexer->temp_word);
    return 0;
  }
  return 1;

}

void ryvm_lexer_free(struct ryvm_lexer *lexer) {
  memory_array_builder_free(&lexer->temp_word);
  memory_free(lexer->mem);
  memory_free(lexer->temp_mem);

}

struct ryvm_token ryvm_lexer_get_token(struct ryvm_lexer *lexer) {
  if(lexer->has_unparsed_token) {
    lexer->has_unparsed_token = 0;
    return lexer->unparsed_token;
  }
  
  struct ryvm_token tok;
  tok.d.reg = 1; //only used to remove CLANG warning for uninitialized struct;


  //this is used so that we can tokenize the LF character even when the newline
  //was skipped over when reading a word
  if(lexer->reached_end_line) {
    lexer->reached_end_line = 0; //set back to 0.
    tok.tag = RYVM_TOKEN_LF;
    return tok;
  }

  char start_char = ryvm_lexer_read_first_nonspace_char(lexer);
  if(start_char == EOF) {
    tok.tag = RYVM_TOKEN_EOF;
    return tok;
  }
  if(start_char == '\n') {
    tok.tag = RYVM_TOKEN_LF;
    return tok;
  }


  
  
  #define RYVM_LEXER_MAC_GRAB_WORD(lexer, start_char, tok, word, word_len) \
    memory_reset(lexer->temp_mem); \
    if(!ryvm_lexer_read_word(lexer, start_char)) {lexer->lexer_failed = 1; return tok;} \
    word = memory_string_builder_finish_build_and_copy_string(&lexer->temp_word, lexer->temp_mem); \
    if(word == NULL) {lexer->lexer_failed = 1; return tok;} \
    word_len = strlen(word);

  char *word;
  uint32_t word_len;

  //note that every token always starts with a sigil, whether that is
  // . @ " # = or even S, D, Q, O for registers.
  // This makes these tokens easier to parse from other data.
  switch(start_char) {

    
    //section
    case '.': {
      RYVM_LEXER_MAC_GRAB_WORD(lexer, start_char, tok, word, word_len)

      if     (strcmp(word, RYVM_CONFIG_MAX_STACK) == 0)        tok.tag = RYVM_TOKEN_SECTION_MAX_STACK_SIZE; 
      else if(strcmp(word, RYVM_CODE_HEADER) == 0)             tok.tag = RYVM_TOKEN_SECTION_TEXT;  
      else if(strcmp(word, RYVM_DATA_HEADER) == 0)             tok.tag = RYVM_TOKEN_SECTION_DATA;   
      else if(strcmp(word, RYVM_DATA_ASCIZ_HEADER) == 0)       tok.tag = RYVM_TOKEN_SECTION_DATA_ASCIZ;   
      else if(strcmp(word, RYVM_DATA_BYTE_HEADER) == 0)        tok.tag = RYVM_TOKEN_SECTION_DATA_BYTE;
      else if(strcmp(word, RYVM_DATA_2BYTE_HEADER) == 0)       tok.tag = RYVM_TOKEN_SECTION_DATA_DOUBLE;
      else if(strcmp(word, RYVM_DATA_4BYTE_HEADER) == 0)       tok.tag = RYVM_TOKEN_SECTION_DATA_QUAD;
      else if(strcmp(word, RYVM_DATA_8BYTE_HEADER) == 0)       tok.tag = RYVM_TOKEN_SECTION_DATA_OCT;

      //invalid section
      else lexer->lexer_failed = 1; 

      return tok;
      //https://www.realdigital.org/doc/be12d58c3c6e45932a27e8c034e821a4
      //https://developer.arm.com/documentation/100068/0624/Migrating-from-armasm-to-the-armclang-Integrated-Assembler/Data-definition-directives

      //https://www.cs.emory.edu/~cheung/Courses/255/Syllabus/7-ARM/data-section.html
    }
    
    //string literal
    case '"': {
      if(!ryvm_lexer_read_string(lexer)) {
        lexer->lexer_failed = 1;
        return tok;
      }

      //string literals and labels must be stored in lexer's main memory.
      tok.d.string_lit = memory_string_builder_finish_build_and_copy_string(&lexer->temp_word, lexer->mem); 
      if(tok.d.string_lit == NULL) {
        lexer->lexer_failed = 1;
        return tok;
      }

      tok.tag = RYVM_TOKEN_STRING_LITERAL;
      return tok;
    }


    //comment
    //comments are already consumed by the read_char function automatically.
   // case ';': ryvm_lexer_consume_comment()

    //label
    // all label defintions start with ':' and are immediately followed by non-whitespace ASCII characters
    case ':': {
      RYVM_LEXER_MAC_GRAB_WORD(lexer, start_char, tok, word, word_len)

      //string literals and labels must be stored in lexer's main memory.
      tok.d.label_name = memory_alloc(lexer->mem, word_len); //note that since @ is removed, this will fit the label, including the NULL character
      tok.tag = RYVM_TOKEN_LABEL;
      if(tok.d.label_name == NULL) {
        lexer->lexer_failed = 1;
        return tok;
      }

      //dont include the sigil
      strcpy(tok.d.label_name, word+1);

      return tok;
    }

    // A signed PC-relative offset to label. The number of bytes of the offset depends on either 
    // 1. The data type it is used with
    // 2. The instruction opcode it is used with
    // a "macro" that replaces #label with the PC-relative signed offset
    // to the original :label defintion. Will throw an error if this label is out of bounds.
    case '#': {
      RYVM_LEXER_MAC_GRAB_WORD(lexer, start_char, tok, word, word_len)

      //string literals and labels must be stored in lexer's main memory.
      tok.d.label_name = memory_alloc(lexer->mem, word_len); //note that since = is removed, this will fit the label, including the NULL character
      tok.tag = RYVM_TOKEN_LABEL_PC_OFF_EXPR;

      if(tok.d.label_name == NULL) {
        lexer->lexer_failed = 1;
        return tok;
      }

      //dont include the sigil
      strcpy(tok.d.label_name, word+1);

      return tok;
    }

    // address-of label
    // Returns a full 8-byte address where a label is.
    // Mostly useful in data entries when creating literal pools.
    case '@': {
      RYVM_LEXER_MAC_GRAB_WORD(lexer, start_char, tok, word, word_len)

      //string literals and labels must be stored in lexer's main memory.
      tok.d.label_name = memory_alloc(lexer->mem, word_len); //note that since = is removed, this will fit the label, including the NULL character
      tok.tag = RYVM_TOKEN_LABEL_ADR_OF_EXPR;

      if(tok.d.label_name == NULL) {
        lexer->lexer_failed = 1;
        return tok;
      }

      //dont include the sigil
      strcpy(tok.d.label_name, word+1);

      return tok;
    }

    //read as negative number
    case '-':  {
      RYVM_LEXER_MAC_GRAB_WORD(lexer, start_char, tok, word, word_len)

      uint8_t type = ryvm_lexer_parse_as_number(word+1, word_len-1, &tok.d.num, 1);
      if(type == 1) {
        tok.tag = RYVM_TOKEN_INT_LITERAL;
        return tok;
      } else if(type == 2) {
        tok.tag = RYVM_TOKEN_FLOAT_LITERAL;
        return tok;
      } 

      lexer->lexer_failed = 1;
      return tok;

    }

    //opcodes (only letters) or register (letter and number) or numeric literal (only digits)
    default: {
      RYVM_LEXER_MAC_GRAB_WORD(lexer, start_char, tok, word, word_len)

      if(word_len == 0) {
        lexer->lexer_failed = 1;
        return tok;
      }

      //is a numeric literal
      if(isdigit(word[0])) {
        uint8_t type = ryvm_lexer_parse_as_number(word, word_len, &tok.d.num, 0);
        if(type == 1) {
          tok.tag = RYVM_TOKEN_INT_LITERAL;
          return tok;
        } else if(type == 2) {
          tok.tag = RYVM_TOKEN_FLOAT_LITERAL;
          return tok;
        } 

        lexer->lexer_failed = 1;
        return tok;
      }

      uint8_t is_reg = 0;
      uint8_t reg_num = 0;
      uint8_t reg_access_num = 0;

      
      //check if this is a "shortcut" name for a register
      // PC is W63
      // FP is W60
      // SP is W61
      // SR is W62
      if(word_len == 2) {
        if(word[0] == 'P' && word[1] == 'C') {
          is_reg = 1;
          reg_access_num = 3; //always the max bytewidth
          reg_num = RYVM_PC_REG;
        } else if(word[0] == 'S' && word[1] == 'P') {
          is_reg = 1;
          reg_access_num = 3; //always the max bytewidth
          reg_num = RYVM_SP_REG;
        } else if(word[0] == 'S' && word[1] == 'F') {
          is_reg = 1;
          reg_access_num = 3; //always the max bytewidth
          reg_num = RYVM_SF_REG;
        } else if(word[0] == 'F' && word[1] == 'P') {
          is_reg = 1;
          reg_access_num = 3; //always the max bytewidth
          reg_num = RYVM_FP_REG;
        } else if(word[0] == 'L' && word[1] == 'R') {
          is_reg = 1;
          reg_access_num = 3; //always the max bytewidth
          reg_num = RYVM_LR_REG;
        }
      }


      //if we have not found a register yet, 
      //check for general registers (a letter followed by a number)
      if(!is_reg && ((word_len == 2 && isdigit(word[1])) || (word_len == 3 && isdigit(word[1]) && isdigit(word[2])))) {
        reg_num = atoi(word+1);
        is_reg = 1;

        switch(word[0]) {
          case 'E': reg_access_num = 0; break;
          case 'Q': reg_access_num = 1; break; 
          case 'H': reg_access_num = 2; break;
          case 'W': reg_access_num = 3; break;
          default: is_reg = 0; 
        }
      }

      if(is_reg) {
        //error
        if(reg_num > 64) {
          lexer->lexer_failed = 1;
          return tok;
        }

        tok.tag = RYVM_TOKEN_REGISTER;
        tok.d.reg = reg_num;
        tok.d.reg |= reg_access_num << 6; // 0b00000011 << 6 = 0b11000000 

        return tok;
      }

      enum ryvm_opcode res;
      if(ryvm_opcode_str_to_op(word, &res)) {
        tok.tag = RYVM_TOKEN_OPCODE;
        tok.d.opcode = res;
        return tok;
      }


      lexer->lexer_failed = 1;
      return tok;
    }



  }


}
