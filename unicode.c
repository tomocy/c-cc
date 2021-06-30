#include "cc.h"

int encode_to_utf8(char* dst, uint32_t code) {
  // 0xxxxxxx
  if (code <= 0x7F) {
    dst[0] = 0b01111111 & code;
    return 1;
  }

  // 110xxxxx 10xxxxxx
  if (code <= 0x7FF) {
    dst[0] = 0b11000000 | (0b00011111 & (code >> 6));
    dst[1] = 0b10000000 | (0b00111111 & code);
    return 2;
  }

  // 1110xxxx 10xxxxxx 10xxxxxx
  if (code <= 0xFFFF) {
    dst[0] = 0b11100000 | (0b00001111 & (code >> 12));
    dst[1] = 0b10000000 | (0b00111111 & (code >> 6));
    dst[2] = 0b10000000 | (0b00111111 & code);
    return 3;
  }

  // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
  dst[0] = 0b11110000 | (0b00000111 & (code >> 18));
  dst[1] = 0b10000000 | (0b00111111 & (code >> 12));
  dst[2] = 0b10000000 | (0b00111111 & (code >> 6));
  dst[3] = 0b10000000 | (0b00111111 & code);
  return 4;
}