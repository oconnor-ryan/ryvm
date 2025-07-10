#include <stdio.h>
#include <stdlib.h>
#include "src/assembler.h"
#include "src/vm.h"

int main(int argc, char **argv) {

  if(argc != 3) {
    printf("Must have 2 arguments!\n");
    return 1;
  }

  //grab file from argv

  FILE *in = fopen(argv[1], "r");
  if(in == NULL) {
    printf("Cannot open input file %s\n", argv[1]);
    return 1;
  }
  FILE *out = fopen(argv[2], "w");
  if(out == NULL) {
    fclose(in);
    printf("Cannot open output file %s\n", argv[2]);
    return 1;
  }

  if(!ryvm_assemble_to_bytecode(in, out)) {
    printf("Cannot assembly bytecode!\n");
    fclose(in);
    fclose(out);
    return 1;
  }

  fclose(in);
  fclose(out);

  
  FILE *bytecode = fopen(argv[2], "r");
  if(bytecode == NULL) {
    printf("Cannot open output file %s\n", argv[2]);
  }

  struct ryvm vm;
  if(!ryvm_vm_load(&vm, bytecode)) {
    printf("Error while loading RYC file!\n");
    fclose(bytecode);
    return 1;
  }

  //we loaded program into memory, no need to read from input file anymore
  fclose(bytecode);

  printf("Program result: %lld\n", ryvm_vm_run(&vm));
  ryvm_vm_free(&vm);

  


  return 0;
}
