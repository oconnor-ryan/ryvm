#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/assembler.h"
#include "../src/vm.h"


/*

#ifdef _WIN32
  const char PATH_SEP = '\\';
#else 
  const char PATH_SEP = '/';
#endif 


void split_basename_and_path(const char *filepath, char *file_parent_dir, char *basename) {
  //reach end of string.
  char *index = filepath;
  char *last_sep = NULL;

  while(*index != '\0') {
    if(*index == PATH_SEP) {
      last_sep = index;
    }
    index++;
  }


  //assume that the file is in the format "basename"
  if(last_sep == NULL) {
    file_parent_dir = NULL;
    basename = filepath;
    return;
  }

  //path like this: /dir1/dir2/file.ryasm (Unix)
  // or C:\dir1\dir2\file.ryasm (Windows)

  file_parent_dir = filepath;
  basename = last_sep + 1;

  //replace last path_sep with \0 to terminate parent directory
  *last_sep = '\0';
}

*/


int main(int argc, const char **argv) {
  
  //TODO: Bugs Found
  /*
    1. Add semantic checking to LDC, LDR and other instructions that require the full 8 bytes of some register arguments
    1. When using bytewidths that are not 8 bytes, issues occur with arithmetic.

  */
  for(int i = 1; i < argc; i++) {


    const char *filename = argv[i];

    size_t filename_len = strlen(filename)+1;
    char *output_filename = malloc(filename_len + 4); //add 4 to append .ryc to output file
    if(output_filename == NULL) {
      printf("Cannot allocate enough memory for test!\n");
      break;
    }
    //copy input filepath to output
    memcpy(output_filename, filename, filename_len);


    //append 4 characters to mark as compiled file
    output_filename[filename_len-1] = '.';
    output_filename[filename_len] = 'r';
    output_filename[filename_len+1] = 'y';
    output_filename[filename_len+2] = 'c';




    //to clear the contents of the file with standard C, we have constantly
    //open the file with the "w" option to clear its contents.

    FILE *out = fopen(output_filename, "w+");
    if(out == NULL) {
      printf("Cannot print temporary file!\n");
      free(output_filename);
      return 1;
    }


    FILE *in = fopen(filename, "r");

    if(!ryvm_assemble_to_bytecode(in, out)) {
      printf("Failed to assembly bytecode for file %s!\n", filename);
      fclose(in);
      fclose(out);
      free(output_filename);
      return 1;
    }

    //dont forget to fseek back to beginning of file...
    fseek(out, 0, SEEK_SET);

    struct ryvm vm;
    printf("======= START PROGRAM FOR %s =======\n", argv[i]);
    ryvm_vm_load(&vm, out);
    printf("Program result: %lld\n", ryvm_vm_run(&vm));
    printf("======= END PROGRAM FOR %s =======\n\n", argv[i]);
    ryvm_vm_free(&vm);



    fclose(in);
    fclose(out);
    free(output_filename);

    //remove(out);

  }



  return 0;
}

