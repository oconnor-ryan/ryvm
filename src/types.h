#ifndef TYPES_HELPER_H
#define TYPES_HELPER_H

#include <stdint.h>

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
