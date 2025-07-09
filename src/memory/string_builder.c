
#include "string_builder.h"
#include <string.h>



//initialize and allocate memory for the string builder.
//returns zero on failure to allocate memory for builder
int memory_string_builder_init(struct memory_array_builder* uninit_builder, size_t char_capacity, enum memory_allocator_tag alloc_type) {
  return memory_array_builder_init(uninit_builder, char_capacity, 1, alloc_type); 
}

//append a single char to the array.
//returns zero on failure to allocate memory for element
int memory_string_builder_append_char(struct memory_array_builder* builder, char c) {
  return memory_array_builder_append_element(builder, &c);
}

//appends all chars inside string into the string builder
//returns zero on failure to allocate memory for all elements
int memory_string_builder_append_string(struct memory_array_builder* builder, char *str) {
  return memory_array_builder_append_elements_from_array(builder, str, strlen(str)); //dont include null character \0
}


//once you finish building the string, copy its contents contiguously into the dest allocator,
//then reset the string builder and return a pointer to the copied string
char* memory_string_builder_finish_build_and_copy_string(struct memory_array_builder* builder, struct memory_allocator *dest) {
  //we can finally append the \0 at the end of the char array.
  if(!memory_string_builder_append_char(builder, '\0')) {
    return NULL;
  }

  //now that we have a c string, copy it to dest
  return memory_array_builder_finish_build_and_copy_array(builder, dest);
}

//reset the size of the string to 0 so that you can reuse it to start making another array.
//If you want to deallocate the builder from memory, use memory_array_builder_free instead
void memory_string_builder_reset(struct memory_array_builder* builder) {
  memory_array_builder_reset(builder, 1);
}

