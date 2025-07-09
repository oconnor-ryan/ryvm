#include "array_builder.h"
#include <assert.h>

int memory_array_builder_init(struct memory_array_builder* uninit_builder, size_t capacity, const size_t element_size, enum memory_allocator_tag alloc_type) {
  uninit_builder->mem = memory_create(capacity, alloc_type);
  if(uninit_builder->mem == NULL) {
    return 0;
  }
  uninit_builder->array_length = 0;
  uninit_builder->element_size = element_size;
  return 1;
}


int memory_array_builder_append_element(struct memory_array_builder* builder, void *element) {
  if(!memory_alloc_and_insert(builder->mem, builder->element_size, element)) {
    return 0;
  }
  builder->array_length++;
  return 1;
}

int memory_array_builder_append_elements_from_array(struct memory_array_builder* builder, void *arr, size_t arr_length) {
  char *array_start = (char *) arr;
  for(size_t i = 0; i < arr_length; i++) {
    if(!memory_array_builder_append_element(builder, array_start + i)) {
      return 0;
    }
  }
  return 1;
}

void* memory_array_builder_get_element_at(struct memory_array_builder* builder, unsigned int index) {
  if(index >= builder->array_length) {
    return NULL;
  }
  switch (builder->mem->tag) {
    case MEMORY_ALLOCATOR_REGION_REALLOC:
      return builder->mem->d.region_realloc.data_ptr + (index * builder->element_size);
    case MEMORY_ALLOCATOR_REGION_LINKED_LIST: {
      //each element is a contiguous block that is stored inside 1 or more non-contiguous
      //nodes. We need to check each node, find the number of elements stored inside each node,
      //and use that information to figure out which node contains the element at the specified
      //index. We will also need to determine the element's location inside the node itself.

      struct memory_region_linked_list *r = &builder->mem->d.region_linked_list;

      //the sum of all elements in each tranversed node
      unsigned int num_elements_checked = 0;

      void *rtn = NULL;

      while(r != NULL) {
        //note that current_size is ALWAYS divisable by element_size, assuming the 
        //array_builder is being used correctly.
        unsigned int num_elements = r->current_size / builder->element_size; //number of elements inside this node
        unsigned int old_num_elements_checked = num_elements_checked;
        num_elements_checked += num_elements;

        //if this node does contain the element at the specified index 
        if(num_elements_checked > index) {
          //figure out the element's pointer by calculating it's offset
          //to the data_ptr of the current node.
          unsigned int element_index_inside_node = index - old_num_elements_checked;
          rtn = r->data_ptr + (element_index_inside_node * builder->element_size);
          break;
        }

        r = r->next;
      }

      return rtn;
    }
    default: assert(0);
  }
}


void* memory_array_builder_finish_build_and_copy_array(struct memory_array_builder* builder, struct memory_allocator *dest) {
  void *copy_arr_ptr = memory_copy(dest, builder->mem);
  if(copy_arr_ptr == NULL) {
    return NULL;
  }
  //just reuse the builders current element size
  memory_array_builder_reset(builder, builder->element_size);
  return copy_arr_ptr;
}

void memory_array_builder_reset(struct memory_array_builder* builder, size_t new_element_size) {
  builder->array_length = 0;
  builder->element_size = new_element_size;
  memory_reset(builder->mem);
}

void memory_array_builder_free(struct memory_array_builder* builder) {
  memory_free(builder->mem);
}
