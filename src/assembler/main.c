#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "assembler.h"

int main(int argc, char **argv) {

  if(argc != 3) {
    printf("Must have 2 arguments!\n");
    return 1;
  }

  if(strcmp(argv[1], argv[2]) == 0) {
    printf("The input and output files cannot be identical!\n");
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

  return 0;
}
