#include "helper.h"
#include <string.h>
#include <assert.h>


float ryvm_vm_helper_reg_to_float(uint64_t reg_value, uint8_t bytewidth) {
  assert(bytewidth == 1 || bytewidth == 2 || bytewidth == 4 || bytewidth == 8);

  float res;

  //if we want all 8 bytes of register
  if(bytewidth >= 4) {
    double d;
    memcpy(&d, &reg_value, 8);
    res = (float) d; //precison loss, but we dont care
  }
  //TODO: Implement half precision floating point and come up with a 8-bit "minifloat"
  else {
    memcpy(&res, &reg_value, 4);
  }

  return res;
}

double ryvm_vm_helper_reg_to_double(uint64_t reg_value, uint8_t bytewidth) {
  assert(bytewidth == 1 || bytewidth == 2 || bytewidth == 4 || bytewidth == 8);

  double res;

  //if we want all 8 bytes of register
  if(bytewidth >= 4) {
    memcpy(&res, &reg_value, 8);
  }
  //TODO: Implement half precision floating point and come up with a 8-bit "minifloat"
  else {
    float f;
    memcpy(&f, &reg_value, 4);
    res = (double) f;
  }

  return res;
}

int64_t ryvm_vm_helper_sign_extend_64(uint8_t *bytes, uint8_t num_bytes) {
  assert(num_bytes <= 8);
  int64_t res;

  memcpy(((uint8_t*) &res) + (8 - num_bytes), bytes, num_bytes);
  res >>= (8 * (8 - num_bytes));
  return res;
  /*
  memcpy(&res, bytes, num_bytes);

  //for the remaining unset bytes, if the most significant byte's sign bit is 1, sign extend 
  //the result by filling all other bytes with 0xFF. If the sign bit is 0, sign extend by filling
  //the remaining bytes with zeros.
  memset(((uint8_t*) &res) + num_bytes, bytes[num_bytes] & 128 ? 0xFF : 0, 8 - num_bytes);

  */
  //a possible non-branching solution is to insert the bytes at the highest position of the
  //result using memcpy. Then if you shift right, because result is signed, it will automatically sign extend
  /*
    int64_t res;
    memcpy(&res, bytes, num_bytes);
    res <<= (8 * (8 - num_bytes)) //move bytes so that they are moved to the most significant byte.
    res >>= (8 * (8 - num_bytes)) //since res is signed, the shift will automatically sign extend the sign bit 
  */
  
}

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
