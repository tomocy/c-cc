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

uint32_t decode_from_utf8(char** dst, char* src) {
  unsigned char* encoded = (unsigned char*)src;

  if (*encoded <= 0x7F) {
    *dst = src + 1;
    return 0b01111111 & *src;
  }

  int len = 0;
  uint32_t code = 0;
  if (*encoded >= 0b11110000) {
    len = 4;
    code = 0b00000111 & *src;
  } else if (*encoded >= 0b11100000) {
    len = 3;
    code = 0b00001111 & *src;
  } else if (*encoded >= 0b11000000) {
    len = 2;
    code = 0b00011111 & *src;
  } else {
    error("invalid UTF-8 sequence");
  }

  for (int i = 1; i < len; i++) {
    if (encoded[i] >> 6 != 0b10) {
      error("invalid UTF-8 sequence");
    }

    code = (code << 6) | (0b00111111 & src[i]);
  }

  *dst = src + len;
  return code;
}