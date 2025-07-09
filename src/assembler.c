#include "assembler.h"
#include "lexer.h"
#include <assert.h>
#include <string.h>
#include "helper.h"
#include <stdlib.h>

//the smallest negative offset for a PC-relative offset
#define MAX_PC_REL_OFFSET_NEG 32768

//the largest positive offset for a PC-relative offset
#define MAX_PC_REL_OFFSET_POS 32767


/*
  The purpose of this compiler is to take human-readable RyByteCode and serialize it into its binary form. 

  The .ry is the Rylang source file, the .ryc is the compiled binary RyByteCode, and .ryasm is the human 
  readable RyByteCode.

  We will also need to differentiate between setting up a VM to interpret ry-bytecode and
  assembling ry-bytecode into native assembly or machine code.
*/




//returns 0 if offset is out of range of the maximum offset size. Returns 1 if offset was successfully calculated
//and in range. 
int ryvm_assembler_relative_pc_offset(uint64_t a, uint64_t dest, int16_t *offset) {
  // need to add positive offset value to 'a' to get to dest.
  if(dest > a) {
    uint64_t off64 = dest - a;
    *offset = (int16_t) off64;
    return off64 <= MAX_PC_REL_OFFSET_POS;
  }

  //need to add negative offset value from a to get to dest
  uint64_t off64 = a - dest;
  *offset = - (int64_t) off64;
  return off64 <= MAX_PC_REL_OFFSET_NEG;
}

void ryvm_assembler_write_and_update_address(struct ryvm_assembler_state *asm_state, uint64_t num_bytes, void *src_ptr) {
  fwrite(src_ptr, num_bytes, 1, asm_state->output);
  asm_state->current_relative_address += num_bytes;
}


void ryvm_assembler_print_register(uint8_t reg) {
  uint8_t reg_num = reg;
  reg_num &= 63; //0b00111111; //clear the 2 MSB 

  //extract the 2MSB from reg and shift them to the LSB
  uint8_t access_width = reg >> 6;
  char c;
  if     (access_width == 0) c = 'E';
  else if(access_width == 1) c = 'Q';
  else if(access_width == 2) c = 'H';
  else if(access_width == 3) c = 'W';
  else assert(0);

  printf("%c%d", c, reg_num);

}

void ryvm_assembler_print_data_entry(struct ryvm_assembler_data_entry *entry) {
  printf("Data Entry: <Type: %d, Value=", entry->tag);

  switch(entry->tag) {
    case RYVM_ASSEMBLER_DATA_ENTRY_TYPE_ASCII_Z: printf("\"%s\"", entry->d.ascii); break;
    case RYVM_ASSEMBLER_DATA_ENTRY_TYPE_1BYTE: printf("%d", entry->d.num.u8); break;
    case RYVM_ASSEMBLER_DATA_ENTRY_TYPE_2BYTE: printf("%d", entry->d.num.u16); break;
    case RYVM_ASSEMBLER_DATA_ENTRY_TYPE_4BYTE: printf("%d", entry->d.num.u32); break;
    case RYVM_ASSEMBLER_DATA_ENTRY_TYPE_8BYTE: printf("%lld", entry->d.num.u64); break;
    case RYVM_ASSEMBLER_DATA_ENTRY_TYPE_2BYTE_FLOAT: printf("%f", entry->d.num.f16); break;
    case RYVM_ASSEMBLER_DATA_ENTRY_TYPE_4BYTE_FLOAT: printf("%f", entry->d.num.f32); break;
    case RYVM_ASSEMBLER_DATA_ENTRY_TYPE_8BYTE_FLOAT: printf("%lf", entry->d.num.f64); break;

    default: assert(0);
  }

  printf(">\n");
}


void ryvm_assembler_print(struct ryvm_assembler_state *asm_state) {
  printf("Config: \n");
  printf("  Max Stack Size Bytes: %lld\n", asm_state->config.max_stack_size);
  printf("Data (%lld Bytes): \n", asm_state->relative_address_text_section);
  
  for(uint64_t i = 0; i < asm_state->data.array_length; i++) {
    struct ryvm_assembler_data_entry *entry = memory_array_builder_get_element_at(&asm_state->data, i);
    printf("  ");
    ryvm_assembler_print_data_entry(entry);
  }

  printf("\nText (%lld Bytes): \n", asm_state->sizeof_text_section);

  for(uint64_t i = 0; i < asm_state->text.array_length; i++) {
    struct ryvm_assembler_text_entry *entry = memory_array_builder_get_element_at(&asm_state->text, i);
    printf("  ");
    if(entry->is_ins) {
      enum ryvm_opcode op = (enum ryvm_opcode) entry->d.ins.opcode;
      const char *ins_name = ryvm_opcode_op_to_str(op);
      printf("%s ", ins_name);
      if(op == RYVM_OP_PCA || op == RYVM_OP_LDI) {
        ryvm_assembler_print_register(entry->d.ins.regs[0]);
        printf(" ");
        int16_t *offset = (int16_t*) (entry->d.ins.regs + 1);
        printf("%d ", *offset);
      } else {
        uint8_t arity = ryvm_opcode_get_arity(op);
        for(uint8_t j = 0; j < arity; j++) {
          ryvm_assembler_print_register(entry->d.ins.regs[j]);
          printf(" ");
        }
      }
      printf("\n");

    } else {
      ryvm_assembler_print_data_entry(&entry->d.data);
    }

  }




}


void ryvm_assembler_error(struct ryvm_assembler_state *asm_state, char *err_message) {
  asm_state->failed = 1;
  printf("Error at Line %lld Col %lld:\n  %s\n", asm_state->lex.source_row, asm_state->lex.source_col, err_message);
}

void ryvm_assembler_free(struct ryvm_assembler_state *asm_state) {
  memory_array_builder_free(&asm_state->data);
  memory_array_builder_free(&asm_state->text);
  memory_array_builder_free(&asm_state->labels);
  memory_array_builder_free(&asm_state->reloc_entries);

  memory_free(asm_state->mem);
  ryvm_lexer_free(&asm_state->lex);

}


int ryvm_assembler_add_data_to_data_section(struct ryvm_assembler_state *asm_state, struct ryvm_assembler_data_entry *e) {
  return memory_array_builder_append_element(&asm_state->data, e);
}

int ryvm_assembler_add_data_to_text_section(struct ryvm_assembler_state *asm_state, struct ryvm_assembler_data_entry *e) {
  struct ryvm_assembler_text_entry t;
  t.is_ins = 0;
  t.d.data = *e;
  return memory_array_builder_append_element(&asm_state->text, &t);
}

struct ryvm_assembler_label* ryvm_assembler_find_label(struct ryvm_assembler_state *asm_state, char *label) {
  for(uint32_t i = 0; i < asm_state->labels.array_length; i++) {
    struct ryvm_assembler_label *label_data = memory_array_builder_get_element_at(&asm_state->labels, i);
    if(strcmp(label, label_data->label) == 0) {
      return label_data;
    }
  }
  return NULL;
}

//pass1
int ryvm_assembler_add_label_def(struct ryvm_assembler_state *asm_state, char *label_name) {
  struct ryvm_assembler_label *label = ryvm_assembler_find_label(asm_state, label_name);

  //if label was already added 
  if(label != NULL) {
    if(label->has_address) {
      ryvm_assembler_error(asm_state, "Duplicate label found!");
      return 0;
    }

    label->has_address = 1;
    label->relative_address = asm_state->current_relative_address;
    return 1;
  }

  struct ryvm_assembler_label new_label;
  new_label.label = label_name;
  new_label.relative_address = asm_state->current_relative_address;
  new_label.has_address = 1;

  if(!memory_array_builder_append_element(&asm_state->labels, &new_label)) {
    ryvm_assembler_error(asm_state, "Cannot allocate enough memory!");
    return 0;
  }
  return 1;
}


int ryvm_assembler_add_label_expr(struct ryvm_assembler_state *asm_state, char *label_name) {
  struct ryvm_assembler_label *label = ryvm_assembler_find_label(asm_state, label_name);

  //if label was already added 
  if(label != NULL) {
    return 1;
  }

  //add label to the symbol table.
  //This allows us to both record all labels and to see if any labels were undefined 
  //after going through the 1st pass.
  struct ryvm_assembler_label new_label;
  new_label.label = label_name;
  new_label.has_address = 0; //add to symbol table, but state that the label was not defined yet

  if(!memory_array_builder_append_element(&asm_state->labels, &new_label)) {
    ryvm_assembler_error(asm_state, "Cannot allocate enough memory!");
    return 0;
  }
  return 1;
}


enum ryvm_assembler_data_entry_type ryvm_assembler_token_data_header_to_data_entry_type(enum ryvm_token_tag tag) {
  switch(tag) {
    case RYVM_TOKEN_SECTION_DATA_BYTE:           return RYVM_ASSEMBLER_DATA_ENTRY_TYPE_1BYTE;
    case RYVM_TOKEN_SECTION_DATA_DOUBLE:         return RYVM_ASSEMBLER_DATA_ENTRY_TYPE_2BYTE;
    case RYVM_TOKEN_SECTION_DATA_QUAD:           return RYVM_ASSEMBLER_DATA_ENTRY_TYPE_4BYTE;
    case RYVM_TOKEN_SECTION_DATA_OCT:            return RYVM_ASSEMBLER_DATA_ENTRY_TYPE_8BYTE;
    case RYVM_TOKEN_SECTION_DATA_FLOAT16_HEADER: return RYVM_ASSEMBLER_DATA_ENTRY_TYPE_2BYTE_FLOAT;
    case RYVM_TOKEN_SECTION_DATA_FLOAT32_HEADER: return RYVM_ASSEMBLER_DATA_ENTRY_TYPE_4BYTE_FLOAT;
    case RYVM_TOKEN_SECTION_DATA_FLOAT64_HEADER: return RYVM_ASSEMBLER_DATA_ENTRY_TYPE_8BYTE_FLOAT;
    default: 
      assert(0);
      return RYVM_ASSEMBLER_DATA_ENTRY_TYPE_8BYTE_FLOAT;
  }
}

uint8_t ryvm_assembler_data_entry_type_bytewidth(enum ryvm_assembler_data_entry_type tag) {
  switch(tag) {
    case RYVM_ASSEMBLER_DATA_ENTRY_TYPE_1BYTE:       return 1;
    case RYVM_ASSEMBLER_DATA_ENTRY_TYPE_2BYTE:       return 2;
    case RYVM_ASSEMBLER_DATA_ENTRY_TYPE_4BYTE:       return 4;
    case RYVM_ASSEMBLER_DATA_ENTRY_TYPE_8BYTE:       return 8;
    case RYVM_ASSEMBLER_DATA_ENTRY_TYPE_2BYTE_FLOAT: return 2;
    case RYVM_ASSEMBLER_DATA_ENTRY_TYPE_4BYTE_FLOAT: return 4;
    case RYVM_ASSEMBLER_DATA_ENTRY_TYPE_8BYTE_FLOAT: return 8;
    default: 
      assert(0);
      return 0;
  }
}

//despite us parsing a list of numbers under one data type, each number will show up in its
//own data entry 
int ryvm_assembler_parse_number_list(struct ryvm_assembler_state *asm_state, enum ryvm_assembler_data_entry_type data_type, int (*add_entry_func)(struct ryvm_assembler_state *s, struct ryvm_assembler_data_entry *e)) {
  uint8_t bytewidth = ryvm_assembler_data_entry_type_bytewidth(data_type);
  uint8_t expect_integer = data_type == RYVM_ASSEMBLER_DATA_ENTRY_TYPE_1BYTE
  || data_type == RYVM_ASSEMBLER_DATA_ENTRY_TYPE_2BYTE
  || data_type == RYVM_ASSEMBLER_DATA_ENTRY_TYPE_4BYTE
  || data_type == RYVM_ASSEMBLER_DATA_ENTRY_TYPE_8BYTE;

  struct ryvm_assembler_data_entry e;
  uint64_t num_nums = 0; 
  while(1) { 
    struct ryvm_token tok = ryvm_lexer_get_token(&asm_state->lex); 
    //check if next token is valid. If not, assume we reached the end of the list of numbers
    //and save this token for future parsing.
    if((expect_integer && tok.tag == RYVM_TOKEN_INT_LITERAL) || (!expect_integer && tok.tag != RYVM_TOKEN_FLOAT_LITERAL)) {
      e.tag = data_type; 
      e.d.num = tok.d.num;
      e.using_placeholder = 0;
    } else if(expect_integer && tok.tag == RYVM_TOKEN_LABEL_PC_OFF_EXPR) {
      e.tag = RYVM_ASSEMBLER_DATA_ENTRY_TYPE_2BYTE;
      if(!ryvm_assembler_add_label_expr(asm_state, tok.d.label_name)) return 0;
      e.d.placeholder = tok;
      e.using_placeholder = 1;
    } else if(expect_integer && tok.tag == RYVM_TOKEN_LABEL_ADR_OF_EXPR) {
      e.tag = RYVM_ASSEMBLER_DATA_ENTRY_TYPE_8BYTE;
      if(!ryvm_assembler_add_label_expr(asm_state, tok.d.label_name)) return 0;
      e.d.placeholder = tok;
      e.using_placeholder = 1;
    } else {
      /* Store token for future parsing */ 
      asm_state->lex.has_unparsed_token = 1; 
      asm_state->lex.unparsed_token = tok; 
      break; 
    } 
   

    if(!add_entry_func(asm_state, &e)) {
      ryvm_assembler_error(asm_state, "Cannot allocate memory."); 
      return 0;
    }
    asm_state->current_relative_address += bytewidth;
    
    num_nums++;
  } 
  if(num_nums == 0) { 
    if(expect_integer) {
      ryvm_assembler_error(asm_state, "Expected integer."); 
    } else {
      ryvm_assembler_error(asm_state, "Expected float."); 
    }
    return 0;
  } 

  return 1;

}


int ryvm_parse_config_entry(struct ryvm_assembler_state *asm_state, struct ryvm_token tok) {
  assert(asm_state->mode == RYVM_ASSEMBLER_MODE_CONFIG);

  if(tok.tag == RYVM_TOKEN_SECTION_DATA) {
    asm_state->mode = RYVM_ASSEMBLER_MODE_DATA;
    return 1;
  }

  if(tok.tag == RYVM_TOKEN_SECTION_TEXT) {
    asm_state->mode = RYVM_ASSEMBLER_MODE_TEXT;
    asm_state->relative_address_text_section = asm_state->current_relative_address;
    return 1;
  }

  if(tok.tag == RYVM_TOKEN_SECTION_MAX_STACK_SIZE) {
    //we already defined a max stack size. Repeat definitions will result in error
    if(asm_state->config.max_stack_size > 0) {
      ryvm_assembler_error(asm_state, "Duplicate .max_stack_size directive!");
      return 0;
    }

    tok = ryvm_lexer_get_token(&asm_state->lex);
    if(asm_state->lex.lexer_failed) {
      ryvm_assembler_error(asm_state, "Lexer failed!");
      return 0;
    }
    if(tok.tag != RYVM_TOKEN_INT_LITERAL) {
      ryvm_assembler_error(asm_state, "Expected integer!");
      return 0;
    }

    asm_state->config.max_stack_size = tok.d.num.u64;
    return 1;
  }

  ryvm_assembler_error(asm_state, "This token is not allowed when parsing in config mode!");
  return 0;
}

int ryvm_parse_data_entry_any_section(struct ryvm_assembler_state *asm_state, struct ryvm_token tok, int (*add_entry_func)(struct ryvm_assembler_state *s, struct ryvm_assembler_data_entry *e)) {
  struct ryvm_assembler_data_entry e;
    
  switch(tok.tag) {
    case RYVM_TOKEN_SECTION_DATA_BYTE:
    case RYVM_TOKEN_SECTION_DATA_DOUBLE:
    case RYVM_TOKEN_SECTION_DATA_QUAD:
    case RYVM_TOKEN_SECTION_DATA_OCT: 
    case RYVM_TOKEN_SECTION_DATA_FLOAT16_HEADER:
    case RYVM_TOKEN_SECTION_DATA_FLOAT32_HEADER:
    case RYVM_TOKEN_SECTION_DATA_FLOAT64_HEADER: {
      ryvm_assembler_parse_number_list(asm_state, ryvm_assembler_token_data_header_to_data_entry_type(tok.tag), add_entry_func);
      if(asm_state->failed) {
        return 0;
      }
      break;
    }

    case RYVM_TOKEN_SECTION_DATA_ASCIZ: {
      tok = ryvm_lexer_get_token(&asm_state->lex);
      if(tok.tag != RYVM_TOKEN_STRING_LITERAL) {
        ryvm_assembler_error(asm_state, "Expected a string literal!");
        return 0;
      }
      e.tag = ryvm_assembler_token_data_header_to_data_entry_type(tok.tag);
      e.d.ascii = tok.d.string_lit; //note that actual string data is stored in lexer's memory
      e.using_placeholder = 0;
      //insert string literal to output file and increase data size.
      if(!add_entry_func(asm_state, &e)) {
        ryvm_assembler_error(asm_state, "Cannot allocate memory!");
        return 0;
      }

      //increment relative address
      asm_state->current_relative_address += strlen(e.d.ascii)+1; //account for null char

      break;
    }

    default:
      //error
      ryvm_assembler_error(asm_state, "Invalid or missing directive or instruction after label.");
      return 0;

  }

  return 1;
}

int ryvm_parse_data_entry(struct ryvm_assembler_state *asm_state, struct ryvm_token tok) {
  assert(asm_state->mode == RYVM_ASSEMBLER_MODE_DATA);


  //we found where the text section starts 
  if(tok.tag == RYVM_TOKEN_SECTION_TEXT) {
    asm_state->mode = RYVM_ASSEMBLER_MODE_TEXT;
    asm_state->relative_address_text_section = asm_state->current_relative_address;
    return 1;
  }

  if(tok.tag == RYVM_TOKEN_LF || tok.tag == RYVM_TOKEN_EOF) {
    return 1;
  }

  
  if(tok.tag == RYVM_TOKEN_LABEL) {
    if(!ryvm_assembler_add_label_def(asm_state, tok.d.label_name)) {
      return 0;
    }

    //skip over all LF tokens
    while((tok = ryvm_lexer_get_token(&asm_state->lex)).tag != RYVM_TOKEN_EOF && tok.tag == RYVM_TOKEN_LF);
  }

  if(!ryvm_parse_data_entry_any_section(asm_state, tok, ryvm_assembler_add_data_to_data_section)) {
    return 0;
  }

  return 1;

}


int ryvm_parse_text_entry(struct ryvm_assembler_state *asm_state, struct ryvm_token tok) {
  assert(asm_state->mode == RYVM_ASSEMBLER_MODE_TEXT);
  if(tok.tag == RYVM_TOKEN_LF || tok.tag == RYVM_TOKEN_EOF) {
    return 1;
  }

  if(tok.tag == RYVM_TOKEN_LABEL) {
    if(!ryvm_assembler_add_label_def(asm_state, tok.d.label_name)) {
      return 0;
    }

    //skip over all LF tokens
    while((tok = ryvm_lexer_get_token(&asm_state->lex)).tag != RYVM_TOKEN_EOF && tok.tag == RYVM_TOKEN_LF);
  }

  //if this is a data entry

  //if this is opcode
  if(tok.tag == RYVM_TOKEN_OPCODE) {

    uint8_t arity = ryvm_opcode_get_arity(tok.d.opcode);


    struct ryvm_assembler_text_entry entry;
    entry.is_ins = 1;

    uint8_t only_reg_args = tok.d.opcode != RYVM_OP_LDI && tok.d.opcode != RYVM_OP_PCA;


    entry.d.ins.opcode = tok.d.opcode;

    if(only_reg_args) {
      entry.d.ins.has_placeholder = 0;
      //gather register tokens based on opcode's arity.
      for(uint8_t i = 0; i < arity; i++) {
        struct ryvm_token tok = ryvm_lexer_get_token(&asm_state->lex);

        if(tok.tag != RYVM_TOKEN_REGISTER) {
          ryvm_assembler_error(asm_state, "Expected Register!");
          return 0;
        }
        entry.d.ins.regs[i] = tok.d.reg;
      }
    }
    // if this accepts a register and a immediate value.
    else {
      struct ryvm_token tok = ryvm_lexer_get_token(&asm_state->lex);
      if(tok.tag != RYVM_TOKEN_REGISTER) {
        ryvm_assembler_error(asm_state, "Expected Register!");
        return 0;
      }
      entry.d.ins.regs[0] = tok.d.reg;


      tok = ryvm_lexer_get_token(&asm_state->lex);
      if(tok.tag == RYVM_TOKEN_INT_LITERAL ) {
        uint16_t *imm = (uint16_t*) (entry.d.ins.regs + 1);
        *imm = tok.d.num.u16;
        entry.d.ins.has_placeholder = 0;
        
      } else if(tok.tag == RYVM_TOKEN_LABEL_PC_OFF_EXPR) {
        entry.d.ins.placeholder = tok;
        entry.d.ins.has_placeholder = 1;
        if(!ryvm_assembler_add_label_expr(asm_state, tok.d.label_name)) return 0;

      } else {
        ryvm_assembler_error(asm_state, "Expected Integer Literal!");
        return 0;
      }
    }

    if(!memory_array_builder_append_element(&asm_state->text, &entry)) {
      ryvm_assembler_error(asm_state, "Cannot allocate memory!");
      return 0;
    }

    asm_state->current_relative_address += 4; //each instruction is 4 bytes

   

    
  } else {
    if(!ryvm_parse_data_entry_any_section(asm_state, tok, ryvm_assembler_add_data_to_text_section)) {
      return 0;
    }
  }


  tok = ryvm_lexer_get_token(&asm_state->lex);
  if(tok.tag != RYVM_TOKEN_EOF && tok.tag != RYVM_TOKEN_LF) {
    ryvm_assembler_error(asm_state, "Expected newline or end of file after instruction!");
    return 0;
  }

  return 1;

}


// for Pass 1, we:
// - Tokenize all data in the source program.
// - Perform parsing and semantic checking.
// - Gather relative addresses for all labels
// - Add all symbol address expressions to relocation table and insert a pointer to the label's symbol table entry.
// - Store all data entries.
// - Store all instructions in list.
// - Check for undefined labels
void ryvm_assembler_pass1(struct ryvm_assembler_state *asm_state) {
  struct ryvm_token tok;
  while((tok = ryvm_lexer_get_token(&asm_state->lex)).tag != RYVM_TOKEN_EOF && !asm_state->lex.lexer_failed) {
    if(tok.tag == RYVM_TOKEN_LF) {
      continue;
    }

    //we have the initial token, so pass it into any of the parser functions

    switch(asm_state->mode) {
      case RYVM_ASSEMBLER_MODE_CONFIG: {
        if(!ryvm_parse_config_entry(asm_state, tok)) {
          asm_state->failed = 1;
          goto stop_parsing;
        }
        break;
      }
      case RYVM_ASSEMBLER_MODE_DATA: {
        if(!ryvm_parse_data_entry(asm_state, tok)) {
          asm_state->failed = 1;
          goto stop_parsing;
        }
        break;
      }
      case RYVM_ASSEMBLER_MODE_TEXT: {
        if(!ryvm_parse_text_entry(asm_state, tok)) {
          asm_state->failed = 1;
          goto stop_parsing;
        }
        break;
      }
      default: assert(0);
    }

  } 

  stop_parsing:
  if(asm_state->lex.lexer_failed) {
    printf("Lexer parsing failed!");
    return;
  }

  if(asm_state->failed) {
    printf("Assembly parsing failed!");
    return;
  }

  //in this pass, we should perform a check for any undefined labels
  for(uint64_t i = 0; i < asm_state->labels.array_length; i++) {
    struct ryvm_assembler_label *label = memory_array_builder_get_element_at(&asm_state->labels, i);
    if(!label->has_address) {
      ryvm_assembler_error(asm_state, "Undefined label!");
      printf("The label expression for \"%s\" is undefined!\n", label->label);
      return;
    }
  }


}

int ryvm_assembler_add_reloc_entry(struct ryvm_assembler_state *state, uint64_t rel_adr_hole, uint64_t rel_adr_value) {
  struct ryvm_assembler_reloc_entry reloc;
  reloc.hole_relative_address = rel_adr_hole;
  reloc.relative_address_value = rel_adr_value;
  if(!memory_array_builder_append_element(&state->reloc_entries, &reloc)) {
    ryvm_assembler_error(state, "Could not allocate enough memory!");
    return 0;
  }

  return 1;
}

/*
  //legend
  b - stands for byte
  b2 - 2 bytes
  b(N) - N bytes


  Binary Format:

  b2 magic_number
  b8 max_stack_size
  b8 data_length_bytes
  b(data_length_bytes) data
  b8 text_length_bytes
  b(text_length_bytes) text
  b8 num_relocs
  b(num_relocs) relocation_entries
*/
void ryvm_assembler_serialize_state_to_file(struct ryvm_assembler_state *state) {
  FILE *f = state->output;

  //magic number
  fwrite("RY", 2, 1, f);

  //max_stack_size
  fwrite(&state->config.max_stack_size, 8, 1, f);


  //data_length_bytes
  fwrite(&state->relative_address_text_section, 8, 1, f); //since data section starts at 0 and ends at text section, the data size matches the starting relative address of the text section


  //write data to file
  for(uint64_t i = 0; i < state->data.array_length; i++) {
    struct ryvm_assembler_data_entry *e = memory_array_builder_get_element_at(&state->data, i);
    if(e->tag == RYVM_ASSEMBLER_DATA_ENTRY_TYPE_ASCII_Z) {
      fwrite(e->d.ascii, strlen(e->d.ascii)+1, 1, f);
    } else {
      fwrite(&e->d.num, ryvm_assembler_data_entry_type_bytewidth(e->tag), 1, f);
    }
  }

  //write length (bytes) of text section
  fwrite(&state->sizeof_text_section, 8, 1, f);

  //write all text entries
  for(uint64_t i = 0; i < state->text.array_length; i++) {
    struct ryvm_assembler_text_entry *e = memory_array_builder_get_element_at(&state->text, i);
    if(e->is_ins) {
      uint8_t ins[4];
      ins[0] = e->d.ins.opcode;
      ins[1] = e->d.ins.regs[0];
      ins[2] = e->d.ins.regs[1];
      ins[3] = e->d.ins.regs[2];

      fwrite(ins, 4, 1, f);
    } else {
      if(e->d.data.tag == RYVM_ASSEMBLER_DATA_ENTRY_TYPE_ASCII_Z) {
        fwrite(e->d.data.d.ascii, strlen(e->d.data.d.ascii)+1, 1, f);
      } else {
        fwrite(&e->d.data.d.num, ryvm_assembler_data_entry_type_bytewidth(e->d.data.tag), 1, f);
      }
    }
  }

  //num relocation entries (we don't need to do bytes since all entries are the same size in bytes)
  fwrite(&state->reloc_entries.array_length, 8, 1, f);


  //write all relocation entries;
  for(uint64_t i = 0; i < state->reloc_entries.array_length; i++) {
    struct ryvm_assembler_reloc_entry *e = memory_array_builder_get_element_at(&state->reloc_entries, i);
    fwrite(&e->hole_relative_address, 8, 1, f);
    fwrite(&e->relative_address_value, 8, 1, f);
  }
}


// At pass2, no parsing/semantic/tokenizing related errors will occur.
// However, an error can still occur with out of range pools.
// For pass2, we:
// - Resolve PC-relative offsets for all PC-relative label expressions (throw error on out-of-bounds offsets)
// - Replace all pointers to label's symbol table entry inside relocation table with their actual relative address.
// - Serialize data entries, pools, and text entries to output file
// - Serialize relocation table to end of output file.
int ryvm_assembler_pass2(struct ryvm_assembler_state *state) {
  uint64_t rel_adr = 0; //data section starts at 0

  //we are going to add relocation entries to all areas where full relative addresses 
  //are required.

  //at the same time, we will evaluate any data entries or .text instructions with placeholders
  //and convert them to real offsets or relative addresses.

  //check data entries for any address/pc-offset placeholders that need to be replaced with actual values.
  for(uint64_t i = 0; i < state->data.array_length; i++) {
    struct ryvm_assembler_data_entry *d = memory_array_builder_get_element_at(&state->data, i);
    if(d->using_placeholder && d->d.placeholder.tag == RYVM_TOKEN_LABEL_ADR_OF_EXPR) {

      struct ryvm_assembler_label *label = ryvm_assembler_find_label(state, d->d.placeholder.d.label_name);
      d->d.num.u64 = label->relative_address;

      if(!ryvm_assembler_add_reloc_entry(state, rel_adr, label->relative_address)) return 0;
     
    }
    if(d->using_placeholder && d->d.placeholder.tag == RYVM_TOKEN_LABEL_PC_OFF_EXPR) {
      struct ryvm_assembler_label *label = ryvm_assembler_find_label(state, d->d.placeholder.d.label_name);
      int16_t offset;

      if(!ryvm_assembler_relative_pc_offset(rel_adr, label->relative_address, &offset)) {
        ryvm_assembler_error(state, "PC-relative offset cannot be calculated since the target label is too far away!");
        printf("The label \"%s\" is too far away from PC %lld, use a literal pool or move the label closer to the instruction!", label->label, rel_adr);
        return 0;
      }
      d->d.num.s16 = offset;

      //reloc entry not needed for PC-relative offsets since the offset remains the same 
    }
    

      
    //update address
    if(d->tag == RYVM_ASSEMBLER_DATA_ENTRY_TYPE_ASCII_Z) {
      rel_adr += strlen(d->d.ascii)+1;
    } else {
      rel_adr += ryvm_assembler_data_entry_type_bytewidth(d->tag);
    }
  }

  //rel_adr = state->relative_address_text_section;


  //update placeholders with relative addresses for text entries
  for(uint64_t i = 0; i < state->text.array_length; i++) {
    struct ryvm_assembler_text_entry *e = memory_array_builder_get_element_at(&state->text, i);
    if(e->is_ins) {
      //try to replace placeholder with real value
      if(e->d.ins.has_placeholder && e->d.ins.placeholder.tag == RYVM_TOKEN_LABEL_PC_OFF_EXPR) {
        struct ryvm_assembler_label *label = ryvm_assembler_find_label(state, e->d.ins.placeholder.d.label_name);
        int16_t offset;

        if(!ryvm_assembler_relative_pc_offset(rel_adr, label->relative_address, &offset)) {
          ryvm_assembler_error(state, "PC-relative offset cannot be calculated since the target label is too far away!");
          printf("The label \"%s\" is too far away from PC %lld, use a literal pool or move the label closer to the instruction!", label->label, rel_adr);
          return 0;
        }

        //apply offset to actual instruction
        int16_t *val = (int16_t*) (e->d.ins.regs + 1);
        *val = offset;
      }

      rel_adr += 4;
    } else {
      if(e->d.data.using_placeholder && e->d.data.d.placeholder.tag == RYVM_TOKEN_LABEL_ADR_OF_EXPR) {
        struct ryvm_assembler_label *label = ryvm_assembler_find_label(state, e->d.data.d.placeholder.d.label_name);
        e->d.data.d.num.u64 = label->relative_address;

        if(!ryvm_assembler_add_reloc_entry(state, rel_adr, label->relative_address)) return 0;

      }
      if(e->d.data.using_placeholder && e->d.data.d.placeholder.tag == RYVM_TOKEN_LABEL_PC_OFF_EXPR) {
        struct ryvm_assembler_label *label = ryvm_assembler_find_label(state, e->d.data.d.placeholder.d.label_name);
        int16_t offset;

        if(!ryvm_assembler_relative_pc_offset(rel_adr, label->relative_address, &offset)) {
          ryvm_assembler_error(state, "PC-relative offset cannot be calculated since the target label is too far away!");
          printf("The label \"%s\" is too far away from PC %lld, use a literal pool or move the label closer to the instruction!", label->label, rel_adr);
          return 0;
        }
        e->d.data.d.num.s16 = offset;
      }

      //update address
      if(e->d.data.tag == RYVM_ASSEMBLER_DATA_ENTRY_TYPE_ASCII_Z) {
        rel_adr += strlen(e->d.data.d.ascii)+1;
      } else {
        rel_adr += ryvm_assembler_data_entry_type_bytewidth(e->d.data.tag);
      }
    }
  }

  ryvm_assembler_print(state);

  //Now that all relative addresses and offsets have been inserted and all relocation entries
  //have been added successfully, we can finally serialize the assembler state to a file.
  ryvm_assembler_serialize_state_to_file(state);

  return 1;



}


void ryvm_assemble_to_bytecode(FILE *in, FILE *out) {
  struct ryvm_assembler_state asm_state;
  asm_state.input = in;
  asm_state.output = out;
  asm_state.config.max_stack_size = 0;
  asm_state.mode = RYVM_ASSEMBLER_MODE_CONFIG;
  asm_state.failed = 0;
  asm_state.current_relative_address = 0; 

  if(!memory_array_builder_init(&asm_state.labels, 100 * sizeof(struct ryvm_assembler_label), sizeof(struct ryvm_assembler_label), MEMORY_ALLOCATOR_REGION_LINKED_LIST)) {
    return;
  }

  if(!ryvm_lexer_init(&asm_state.lex, in)) {
    memory_array_builder_free(&asm_state.labels);
    return;
  }

  asm_state.mem = memory_create(100, MEMORY_ALLOCATOR_REGION_REALLOC);
  if(asm_state.mem == NULL) {
    memory_array_builder_free(&asm_state.labels);
    ryvm_lexer_free(&asm_state.lex);
    return;
  }

  if(!memory_array_builder_init(&asm_state.data, 100 * sizeof(struct ryvm_assembler_data_entry), sizeof(struct ryvm_assembler_data_entry), MEMORY_ALLOCATOR_REGION_LINKED_LIST)) {
    memory_array_builder_free(&asm_state.labels);
    memory_free(asm_state.mem);
    ryvm_lexer_free(&asm_state.lex);
    return;
  }

  if(!memory_array_builder_init(&asm_state.text, 100 * sizeof(struct ryvm_assembler_text_entry), sizeof(struct ryvm_assembler_text_entry), MEMORY_ALLOCATOR_REGION_LINKED_LIST)) {
    memory_array_builder_free(&asm_state.labels);
    memory_array_builder_free(&asm_state.data);
    memory_free(asm_state.mem);
    ryvm_lexer_free(&asm_state.lex);
    return;
  }

  if(!memory_array_builder_init(&asm_state.reloc_entries, 100 * sizeof(struct ryvm_assembler_reloc_entry), sizeof(struct ryvm_assembler_reloc_entry), MEMORY_ALLOCATOR_REGION_LINKED_LIST)) {
    memory_array_builder_free(&asm_state.labels);
    memory_array_builder_free(&asm_state.data);
    memory_array_builder_free(&asm_state.text);
    memory_free(asm_state.mem);
    ryvm_lexer_free(&asm_state.lex);
    return;
  }




  ryvm_assembler_pass1(&asm_state);

  if(asm_state.failed || asm_state.lex.lexer_failed) {
    return;
  }

  asm_state.sizeof_text_section = asm_state.current_relative_address - asm_state.relative_address_text_section;
  
  //ryvm_assembler_print(&asm_state);

  ryvm_assembler_pass2(&asm_state);
  ryvm_assembler_free(&asm_state);
}
