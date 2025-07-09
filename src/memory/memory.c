#include "memory.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>


int memory_region_linked_list_init(struct memory_region_linked_list* region, size_t capacity) {
  region->capacity = capacity;
  region->current_size = 0;
  region->data_ptr = malloc(capacity);
  region->next = NULL; //sentinal value for end of list

  if(region->data_ptr == NULL) {
    free(region);
    return 0;
  }

  return 1;
}

struct memory_region_linked_list* memory_region_linked_list_create(size_t capacity) {
  struct memory_region_linked_list *region = malloc(sizeof(struct memory_region_linked_list));
  if(region == NULL) {
    return NULL;
  }

  //if initialization failed
  if(!memory_region_linked_list_init(region, capacity)) {
    free(region);
    return NULL;
  }

  return region;
}

struct memory_allocator* memory_create(size_t capacity, enum memory_allocator_tag tag) {
  assert(capacity > 0);
  
  struct memory_allocator *alloc = malloc(sizeof(struct memory_allocator));
  if(alloc == NULL) {
    return NULL;
  }

  alloc->tag = tag;

  switch(tag) {
    case MEMORY_ALLOCATOR_REGION_REALLOC: {
      alloc->d.region_realloc.capacity = capacity;
      alloc->d.region_realloc.current_size = 0;
      alloc->d.region_realloc.data_ptr = malloc(capacity);

      if(alloc->d.region_realloc.data_ptr == NULL) {
        free(alloc);
        return NULL;
      }

      break;
    }
    case MEMORY_ALLOCATOR_REGION_LINKED_LIST: {
      //if initializing 1st node of linked list failed
      if(!memory_region_linked_list_init(&alloc->d.region_linked_list, capacity)) {
        free(alloc);
        return NULL;
      }

      break;
    }

    default:
      assert(0);
  }
  return alloc;
}

int memory_region_realloc_expand(struct memory_region_realloc *region, size_t new_size) {

  char *new_ptr = realloc(region->data_ptr, new_size);
  if(new_ptr == NULL) {
    return 0;
  }
  region->capacity = new_size;
  region->data_ptr = new_ptr;
  return 1;

}



struct memory_region_linked_list* memory_region_linked_list_get_available_region(struct memory_region_linked_list *region, size_t size_of_next_allocation) {
  assert(region != NULL); //region should NEVER be null, as the root node should always be available.

  struct memory_region_linked_list *node = region;
  struct memory_region_linked_list *last_node = NULL;

  //check if we have a node with enough space to store the next allocation
  while(node != NULL) {
    if(size_of_next_allocation + node->current_size <= node->capacity) {
      //no need to add another memory region, reuse this one 
      return node;
    }
    last_node = node;
    node = node->next;
  }

  //At this point, we found 0 nodes that can fit this next allocation, so we must create a new node


  //if size of next allocation is larger than our capacity,
  //append new memory region just large enough for that allocation.
  if(size_of_next_allocation > last_node->capacity) {
    last_node->next = memory_region_linked_list_create(size_of_next_allocation);
    if(last_node->next == NULL) {
      return NULL;
    }

    return last_node->next;
  }

  //if our current capacity is sufficient to store the next allocation
  last_node->next = memory_region_linked_list_create(last_node->capacity);

  //next will be NULL if failed to allocate memory.
  return last_node->next;
}



void*  memory_alloc(struct memory_allocator *alloc, size_t size) {
  if(alloc == NULL) {
    return NULL;
  }

  switch(alloc->tag) {
    case MEMORY_ALLOCATOR_REGION_LINKED_LIST: {
      //find a memory region that will fit the next allocation, or allocate another node in the linked list
      //and return it.
      struct memory_region_linked_list *region = memory_region_linked_list_get_available_region(&alloc->d.region_linked_list, size);

      //allocation failed
      if(region == NULL) {
        return NULL;
      }

      //allocate using bump pointer.
      char* start_of_data = region->data_ptr + region->current_size;
      region->current_size += size;

      return (void*) start_of_data;
    }

    case MEMORY_ALLOCATOR_REGION_REALLOC: {
      struct memory_region_realloc *region = &alloc->d.region_realloc;

      //if the next allocation cannoot fit inside the current region, grow the region.
      if(size + region->current_size > region->capacity) {
        size_t new_size = region->capacity;

        //keep doubling new size by 2 until the capacity is large enough
        while(size + region->current_size > new_size) {
          new_size *= 2;
        }

        //if reallocation failed
        if(!memory_region_realloc_expand(region, new_size)) {
          return NULL;
        }
      }

      //allocate using bump pointer.
      char* start_of_data = region->data_ptr + region->current_size;
      region->current_size += size;

      return start_of_data;
    }

    default:
      assert(0);
  }
}


//a convieniance function to allocate memory for and automatically copy a value.
//returns NULL on failure to allocate enough memory
void* memory_alloc_and_insert(struct memory_allocator *alloc, size_t size, void *value) {
  //if we don't check this, we will get undefined behavior from memcpy
  if(value == NULL) {
    return NULL;
  }

  char *ptr = memory_alloc(alloc, size);
  if(ptr == NULL) {
    return NULL;
  }
  memcpy(ptr, value, size);
  return ptr;
}


//remove a certain number of bytes from the most recently allocated memory
void memory_remove(struct memory_allocator *alloc, size_t bytes_to_remove) {
  if(bytes_to_remove == 0) return;

  switch(alloc->tag) {
    case MEMORY_ALLOCATOR_REGION_REALLOC:
      if(bytes_to_remove > alloc->d.region_realloc.current_size) {
        alloc->d.region_realloc.current_size = 0;
      }
      alloc->d.region_realloc.current_size -= bytes_to_remove;
      break;
    case MEMORY_ALLOCATOR_REGION_LINKED_LIST: {
      //to figure out which nodes to remove data from, we must first know
      //the total amount of used data in the allocator.
      size_t total_size = memory_get_total_size(alloc);

      //remove all data from all nodes, equivalent to a reset
      if(bytes_to_remove >= total_size) {
        memory_reset(alloc);
        break;
      }

      /*
        To update the new sizes of each node after removing R bytes, 
        we could do the following if we had a doubly-linked list:

        1. Start at the end of the list:
        2. Check the current node.
        3. If the current node's size S is less than R, subtract R by S and
           set S to 0, then go to the previous node and repeat step 2.
        4. If the current node's size S is greater or equal to R, subtract
           S by R and store the result in S. We have now successfully updated
           the sizes of all nodes in the allocator and can end the function.
      */

      //However, because we cannot reverse iterate though a singly-linked list,
      //we can achieve the same result as the above algorithm using the
      //algorithm below:
      // 1. Calculating the total size of all nodes.
      // 2. If the total size is less than or equal to the number of
      //    bytes being removed, set sizes of all nodes to 0 and exit the function.
      // 3. Otherwise: calculate the new total size after N bytes are removed.
      // 4. Tranverse the list until the sum of the sizes of the 
      //    nodes tranversed is greater or equal to the new total size.
      // 5.  Set size to 0 for all nodes after the last tranversed node.
      // 6. Substract the size of the last tranversed node by the difference
      //    of the (sum of transversed node sizes) and (the new total size)

      // Example: 
      /*
        We have 4 nodes of sizes [1,4,4,1] and we need to remove 2 bytes.

        The total size of the 4 nodes is 1+4+4+1 = 10.

        Since the total_size > bytes_to_remove, we continue on.

        Instead of tracking the number of bytes currently removed 
        like in the 1st algorithm above, we track the number of bytes
        currently saved, since we can only iterate forward.

        The new total size of all the nodes after removing 2 bytes 
        should be 10 - 2 = 8.

        We need to figure out what nodes will be saved
        by tranversing each node until the sum of their sizes is greater 
        or equal to the new total size. The last tranversed node will 
        need their size adjusted to match the new total size.

        We tranverse the nodes starting at the 1st node.
        Node 1: Tranversed Node Size = 0 + 1 = 1
        Node 2: Tranversed Node Size = 1 + 4 = 5
        Node 3: Tranversed Node Size = 5 + 4 = 9

        At the 3rd node, we exceed the new total size of 8. 

        All nodes after the 3rd node have their size set to 0. 
        Node sizes now look like this: [1,4,4,0] 

        We have currently saved 9 bytes from being deleted, but our new total
        size should be 8. To fix this, subtract the size of the 3rd node
        by 9-8 = 1. 

        3rd node size = Old Size - (Bytes Saved - New Total Size) = 4 - (9-8) = 3

        Now our sizes for each node is [1,4,3,0], which adds up to 8.

        Now we have removed 2 bytes from the linked list of memory regions!
      */

      //the node where we need to start removing data from
      struct memory_region_linked_list *node= &alloc->d.region_linked_list;

      //the amount of used memory remaining after removing N bytes
      size_t size_after_removal = total_size - bytes_to_remove;
      
      //the current sum of the sizes of each node traversed.
      size_t size = 0;

      //search for the 1st node that will need some of its data removed
      while(size < size_after_removal) {
        //node should NEVER be NULL since we've already checked
        //that the size_after_removal is less than the total size of 
        //the entire allocator
        assert(node != NULL);

        size += node->current_size;
        node = node->next;
      }

      //note that at this point, size >= size_after_removal.

      //To set the new size of the allocator, we must do 2 things: 

      //1. We set the size of all nodes after the selected node to 0.
      struct memory_region_linked_list *node_to_zero_out= node->next;
      while(node_to_zero_out != NULL) {
        node_to_zero_out->current_size = 0;
        node_to_zero_out = node_to_zero_out->next;
      }

      //2. Subtract the current size of the selected node 
      //by the remaining amount of bytes we need to remove, which
      //is equal to size - size_after_removal.
      node->current_size -= size - size_after_removal;

      //at this point, the sum of all sizes of each node should be
      //the same as size_after_removal

      break;

    }
  }
}


void memory_reset(struct memory_allocator *alloc) {

  switch (alloc->tag) {
    case MEMORY_ALLOCATOR_REGION_LINKED_LIST: {
      struct memory_region_linked_list *r = &alloc->d.region_linked_list;

      //mark all nodes as empty.
      while(r != NULL) {
        r->current_size = 0;
        r = r->next;
      }

      break;
    }

    case MEMORY_ALLOCATOR_REGION_REALLOC: {
      alloc->d.region_realloc.current_size = 0;
      break;
    }

    default:
      assert(0);
  }
  
}


void memory_free(struct memory_allocator *alloc) {
  if(alloc == NULL) {
    return;
  }

  switch (alloc->tag) {
    case MEMORY_ALLOCATOR_REGION_LINKED_LIST: {
      //note that the 1st node in the linked list does not need to be freed since it is 
      //stored inside the allocator object. 
      //All other nodes need to be freed however.
      struct memory_region_linked_list *region = alloc->d.region_linked_list.next;

      while(region != NULL) {
        free(region->data_ptr);
        struct memory_region_linked_list *child = region->next; //grab pointer before it is lost
        free(region);
    
        region = child;
      }

      //don't forget to free the DATA_PTR inside the 1st node of the linked list
      free(alloc->d.region_linked_list.data_ptr);
      
      break;
    }

    case MEMORY_ALLOCATOR_REGION_REALLOC: {
      free(alloc->d.region_realloc.data_ptr);
      break;
    }

    default:
      assert(0);
  }

  //all allocators are stored on heap so the programmer KNOWS that he must always manually free
  //memory_allocator structs.
  free(alloc);
 
}

size_t memory_get_total_size(struct memory_allocator *alloc) {
  switch(alloc->tag) {
    case MEMORY_ALLOCATOR_REGION_REALLOC:
      return alloc->d.region_realloc.current_size;
    case MEMORY_ALLOCATOR_REGION_LINKED_LIST: {
      struct memory_region_linked_list *node = &alloc->d.region_linked_list;
      size_t size = 0;
      while(node != NULL) {
        size += node->current_size;
        node = node->next;
      }
      return size;
    }
  }
}

//copy all used data from src allocator into one contiguous block inside the dest allocator
//returns pointer to start of copied memory inside dest on success, NULL on failure
void* memory_copy(struct memory_allocator *dest, struct memory_allocator *src) {

  //note that upon copying src to dest, all pointers to data in src will be invalidated
  //once src is deallocated. This makes copying from src to dest almost useless unless
  //you were using src to store 1 large value at the start of the malloced block (maybe a large struct or an array
  //of evenly spaced elements). 

  //If using src to hold 1 large value, it would be better to combine all
  //data in src to a contiguous block of memory and add that block it to dest, returning the pointer
  //to the start of the block. 
  //This allows us to retrieve a pointer to the copied block in dest
  //and work with it, even if src is deallocated

  //Thus, when copying memory from src to dest, the src data will be 
  //copied into a single contiguous block inside dest.
  switch(src->tag) {
    case MEMORY_ALLOCATOR_REGION_REALLOC: {
      //allocate everything into single contiguous block
      return memory_alloc_and_insert(dest, src->d.region_realloc.current_size, src->d.region_realloc.data_ptr);
    }
    case MEMORY_ALLOCATOR_REGION_LINKED_LIST: {
      //we will allocate everything into a single contiguous block.

      //allocate the single contiguous block that will fit all of the
      //USED memory in src.
      size_t src_block_size = memory_get_total_size(src);
      char *copy_block_start = (char*) memory_alloc(dest, src_block_size);

      //if allocation failed
      if(copy_block_start == NULL) {
        return NULL;
      }

      struct memory_region_linked_list *node = &src->d.region_linked_list;
      char *copy_block_cur_pos = copy_block_start;

      //copy each node of linked list into dest copy.
      while(node != NULL) {
        memcpy(copy_block_cur_pos, node->data_ptr, node->current_size);
        copy_block_cur_pos += node->current_size;
        node = node->next;
      }
      return copy_block_start;
    }
  }
}

void* memory_get_copy(struct memory_allocator *src) {

  //note that upon copying src to dest, all pointers to data in src will be invalidated
  //once src is deallocated. This makes copying from src to dest almost useless unless
  //you were using src to store 1 large value at the start of the malloced block (maybe a large struct or an array
  //of evenly spaced elements). 

  //Thus, when copying memory from src to dest, the src data will be 
  //copied into a single contiguous block inside dest.


  size_t src_block_size = memory_get_total_size(src);

  void *buffer = malloc(src_block_size);
  if(buffer == NULL) {
    return NULL;
  }


  switch(src->tag) {
    case MEMORY_ALLOCATOR_REGION_REALLOC: {
      //allocate everything into single contiguous block
      memcpy(buffer, src->d.region_realloc.data_ptr, src_block_size); 
      return buffer;
    }
    case MEMORY_ALLOCATOR_REGION_LINKED_LIST: {
      //we will allocate everything into a single contiguous block.

      struct memory_region_linked_list *node = &src->d.region_linked_list;

      //start position at beginning of buffer
      char *copy_block_cur_pos = buffer;

      //copy each node of linked list into dest copy.
      while(node != NULL) {
        memcpy(copy_block_cur_pos, node->data_ptr, node->current_size);
        copy_block_cur_pos += node->current_size;
        node = node->next;
      }
      return buffer;
    }
  }

}
