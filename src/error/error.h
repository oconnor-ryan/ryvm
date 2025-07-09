#ifndef ERROR_H
#define ERROR_H

#include <stdarg.h>
#include <stdint.h>

struct error;
struct error_vtable;


struct error {
  uint64_t source_line;
  uint64_t source_col;
  struct memory_allocator *mem;
  char *error_string;
};




//error handling
int error_init(struct error *error);
void error_free(struct error *error);


//this is an overloadable function
void error_print(struct error *error);
void error_set(struct error *error_obj, uint64_t source_line, uint64_t source_col, const char *message, va_list args);



#endif //ERROR_H
