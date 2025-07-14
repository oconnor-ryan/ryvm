#ifndef TYPES_HELPER_H
#define TYPES_HELPER_H

#include <stdint.h>

//If every element within a union is the same size, no implicit conversion will occur, leaving 
//the bit representation unaltered, though the size restriction limits its usefulness.
union num64 {
  uint64_t i;
  int64_t s;
  double f;
};

union num32 {
  uint32_t i;
  int32_t s;
  float f;
};

//note that if you use this union, accessing values of different sizes will cause an implicit
//conversion. Example, accessing a 32-bit float after you assigned a 64-bit float to it will
//cause the 64-bit float to be implicitly converted to 32-bit rather than reading the preset bits
//as a 32-bit.
union num {
  uint64_t u64;
  int64_t s64;
  double f64;

  uint32_t u32;
  int32_t s32;
  float f32;

  uint16_t u16;
  int16_t s16;
  float f16;

  uint8_t u8;
  int8_t s8;

};




#endif// TYPES_HELPER_H
