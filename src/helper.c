#include "helper.h"
#include <string.h>

int32_t ryvm_vm_helper_cast_int_24_to_32(uint8_t bytes[3]) {
  int32_t value;
  //manually check if sign extension is needed for 24-bit integer.
  uint8_t msb = bytes[2];

  //no need to sign-extend, we only need to 0 extend
  if((msb & 128) == 0) {
    value = 0; 
    memcpy(&value, bytes, 3);
  } 
  //we need to sign extend to 32 bits
  else {
    memcpy(&value, bytes, 3);
    value |= 0xFF000000;
  }

  return value;
}
