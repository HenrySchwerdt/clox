#ifndef clox_utils_h
#define clox_utils_h
#include "common.h"

#define CONVERT_TO_BYTE_ARRAY(numberArray, numberOfBytes, value) toByteArray(numberArray, numberOfBytes, value) 
#define CONVERT_BYTE_ARRAY_TO_INT(byteArray, bytes) byteArrayToInteger(byteArray, bytes)
void toByteArray(uint8_t * splitNumber, uint8_t bytes, int value);
int byteArrayToInteger(uint8_t * byteArray, uint8_t bytes);
#endif