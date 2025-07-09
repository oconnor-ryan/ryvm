#!/bin/bash

#note that the project is written to comply with the C99 standard, with zero reliance on compiler
#extensions.

case $1 in 
  gcc)
    #dont forget -lm to load math.h for gcc
    gcc -Wall -Wextra -pedantic -std=c99 -g \
      $2 $(find ./src -type f -name "*.c") \
      -lm -o ./generated_bins/ryvm
    ;;
  clang)
    #note that -Wall and -Wextra -pedantic is recommended for diagnostics, though -Weverything enables ALL diagnostics.
    #Weverything contains contradicting warnings, so it's not recommended.
    clang -fcolor-diagnostics -fansi-escape-codes -g -std=c99 -Wall -Wextra -pedantic \
      $2 $(find ./src -type f -name "*.c") \
      -o ./generated_bins/ryvm
    ;;
  *)
    echo "No valid compiler specified"
    ;;
esac