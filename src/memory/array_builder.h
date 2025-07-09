#ifndef MEMORY_ARRAY_BUILDER_H
#define MEMORY_ARRAY_BUILDER_H

#include "memory.h"
#include <stdint.h>

struct memory_array_builder {
  struct memory_allocator *mem;
  size_t array_length;
  size_t element_size;
};

//initialize and allocate memory for the array builder.
//returns zero on failure to allocate memory for builder
int memory_array_builder_init(struct memory_array_builder* uninit_builder, size_t capacity, const size_t element_size, enum memory_allocator_tag alloc_type);


//append a single element to the array.
//returns zero on failure to allocate memory for element
int memory_array_builder_append_element(struct memory_array_builder* builder, void *element);

//appends all elements inside arr into the array builder
//returns zero on failure to allocate memory for all elements
int memory_array_builder_append_elements_from_array(struct memory_array_builder* builder, void *arr, size_t arr_length);


//get the element at the specified index of the array.
void* memory_array_builder_get_element_at(struct memory_array_builder* builder, unsigned int index);

//once you finish building the array, copy its contents contiguously into the dest allocator,
//then reset the array builder and return a pointer to the copied array
void* memory_array_builder_finish_build_and_copy_array(struct memory_array_builder* builder, struct memory_allocator *dest);

//reset the size of the array to 0 so that you can reuse it to start making another array.
//If you want to deallocate the builder from memory, use memory_array_builder_free instead
void memory_array_builder_reset(struct memory_array_builder* builder, size_t new_element_size);

//free all data from builder. You cannot use this builder again after calling this.
//If you want to reuse your builder, call memory_array_builder_reset instead.
void memory_array_builder_free(struct memory_array_builder* builder);






#endif // MEMORY_ARRAY_BUILDER_H
