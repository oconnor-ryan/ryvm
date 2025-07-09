#ifndef MEMORY_HEADER_H
#define MEMORY_HEADER_H

#include <stddef.h>

struct memory_region_realloc {
  //because we cannot do pointer arithmetic for void* in C, we must use
  //char*, since the char datatype is always 1 byte, so 
  //the expression (region + 1) will be a pointer to the 1st byte after the region pointer.

  char *data_ptr; //the start of the allocated memory on heap
  size_t capacity;
  size_t current_size;
};

struct memory_region_linked_list {
  //because we cannot do pointer arithmetic for void* in C, we must use
  //char*, since the char datatype is always 1 byte, so 
  //the expression (region + 1) will be a pointer to the 1st byte after the region pointer.

  char *data_ptr; //the start of the allocated memory on heap
  size_t capacity;
  size_t current_size;
  
  //when the next allocation cannot
  //fit in the current memory region, allocate a new one to hold the allocation
  struct memory_region_linked_list *next;
};


enum memory_allocator_tag {
  // a region-based allocator that will copy itself into another 
  //larger memory region if all memory is used.
  //All pointers to this memory become invalid after growing.

  //This is useful if you want to store a contiguous array of data
  //and don't need any pointers to the data during allocation.
  MEMORY_ALLOCATOR_REGION_REALLOC, 

  //a region-based allocator where upon requiring more memory, it will allocate another memory
  //region and place all future allocations there.
  //All pointers will still be valid after growing.

  //This is useful if you want to keep pointers to this data without worrying about them being invalidated
  //or if you don't want to waste processing power on copying all the old data into a new area of memory.
  MEMORY_ALLOCATOR_REGION_LINKED_LIST, 

  //
};

struct memory_allocator {
  enum memory_allocator_tag tag;

  union {
    struct memory_region_realloc region_realloc;
    struct memory_region_linked_list region_linked_list;
  } d;
};

//allocate a new memory_region on the heap with the specified capacity.
struct memory_allocator* memory_create(size_t capacity, enum memory_allocator_tag tag);


//allocate memory from the memory_region. Returns a pointer to the 
//start of the newly allocated memory or NULL if it fails.
void*  memory_alloc(struct memory_allocator *alloc, size_t size);


//allocates enough memory to store a value of a specified size and copies that value into
//the newly allocated memory. Returns a pointer to the copy of the data on success, NULL on failure
void* memory_alloc_and_insert(struct memory_allocator *alloc, size_t value_size, void *value);


//remove a certain number of bytes, starting from the most recently allocated data.
void memory_remove(struct memory_allocator *alloc, size_t bytes_to_remove);

//resets the size of the allocator to 0, making it empty. 
//This "frees" all old memory by allowing new allocations to overwrite old ones.
void memory_reset(struct memory_allocator *alloc);


//permanently free this region of memory, making it unusable for future allocations.
void memory_free(struct memory_allocator *alloc);

//get the total size in bytes of all of the USED memory inside the allocator
size_t memory_get_total_size(struct memory_allocator *alloc);

//copy all USED data from one allocator to another.
//returns pointer to start of copied memory inside dest on success, NULL on failure
void* memory_copy(struct memory_allocator *dest, struct memory_allocator *src);

void* memory_get_copy(struct memory_allocator *src);



#endif // MEMORY_HEADER_H

