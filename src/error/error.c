

#include "error.h"
#include "../logger/logger.h"
#include "../memory/memory.h"
#include <stdio.h>

int error_init(struct error *error) {
  error->error_string = NULL;
  error->mem = memory_create(100, MEMORY_ALLOCATOR_REGION_LINKED_LIST);
  if(error->mem == NULL) {
    return 0;
  }
  error->source_line = 0;
  error->source_col = 0;

  return 1;
}

void error_set(struct error *error, uint64_t source_line, uint64_t source_col, const char *message, va_list args) {

  error->source_line = source_line;
  error->source_col = source_col;

  //va_list args;
  //va_start(args, message);


  //required since when first checking the required buffer size, the state of 
  //args is changed.
  //We need to keep a copy of the state of the va_list before it was modified.
  va_list args_copy;
  va_copy(args_copy, args);


  //A interesting trick to checking the required buffer size to fit the formatted string
  //after all arguments are placed in it.
  size_t required_buffer_size = vsnprintf(NULL, 0, message, args);

  //negative values are encoding errors
  if(required_buffer_size < 0) {
    error->error_string = "Failed to encode error string while formatting error!";

    //cleanup variable length arg state objects
    goto cleanup;

  }

  //add +1 since vsnprintf does NOT include null character with the required size for the buffer.
  required_buffer_size += 1;

  char *dest = memory_alloc(error->mem, required_buffer_size);

  if(dest == NULL) {
    //because we can't even allocate the memory to store the original error,
    //we must use a default memory alloc error using a static string.
    error->error_string = "Cannot allocate data to store error!";

    //cleanup variable length arg state objects
    goto cleanup;
  }

  size_t num_chars_written =  vsprintf(dest, message, args_copy);

  if(num_chars_written < 0) {
    error->error_string = "Failed to encode error string while formatting error!";

    //cleanup variable length arg state objects
    goto cleanup;
  }

  error->error_string = dest;


  cleanup:
  //cleanup variable length arg state objects
  va_end(args_copy);

  //dont clean the args passed into this function, the caller should free args for us.
  //va_end(args);


}

void error_print(struct error *error) {

  logger_log(LOGGER_LEVEL_ERROR, "Error at Line: %d, Col %d \n", error->source_line, error->source_col);

  if(error->error_string == NULL) {
    return;
  }

  logger_log(LOGGER_LEVEL_ERROR, error->error_string);
}

void error_free(struct error *error) {
  memory_free(error->mem);
}
