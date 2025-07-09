#ifndef MEMORY_STRING_BUILDER_H
#define MEMORY_STRING_BUILDER_H

#include "array_builder.h"

//Note that these are just some additional utility functions for 
//memory_array_builder if you want to build a C string.


//initialize and allocate memory for the string builder.
//returns zero on failure to allocate memory for builder
int memory_string_builder_init(struct memory_array_builder* uninit_builder, size_t char_capacity, enum memory_allocator_tag alloc_type);

//append a single char to the array.
//returns zero on failure to allocate memory for element
int memory_string_builder_append_char(struct memory_array_builder* builder, char c);

//appends all chars inside string into the string builder
//returns zero on failure to allocate memory for all elements
int memory_string_builder_append_string(struct memory_array_builder* builder, char *str);


//get a pointer to the current string.
char* memory_string_builder_get_string(struct memory_array_builder* builder);

//once you finish building the string, copy its contents contiguously into the dest allocator,
//then reset the string builder and return a pointer to the copied string
char* memory_string_builder_finish_build_and_copy_string(struct memory_array_builder* builder, struct memory_allocator *dest);

//If you want to deallocate the builder from memory, use memory_string_builder_free instead
void memory_string_builder_reset(struct memory_array_builder* builder);







#endif // MEMORY_STRING_BUILDER_H

