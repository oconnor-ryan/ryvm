#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vm.h"

int main(int argc, char **argv) {

  if(argc != 2) {
    printf("Must have 1 arguments!\n");
    return 1;
  }

  //grab file from argv

  FILE *in = fopen(argv[1], "r");
  if(in == NULL) {
    printf("Cannot open input file %s\n", argv[1]);
    return 1;
  }

  struct ryvm vm;
  if(!ryvm_vm_load(&vm, in)) {
    printf("Error while loading RYC file!\n");
    fclose(in);
    return 1;
  }

  //we loaded program into memory, no need to read from input file anymore
  fclose(in);


  printf("Program result: %lld\n", ryvm_vm_run(&vm));
  ryvm_vm_free(&vm);


  return 0;
}
