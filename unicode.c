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

int encode_to_utf16(uint16_t* dst, uint32_t code) {
  if (code <= 0xFFFF) {
    dst[0] = code;
    return 2;
  }

  code -= 0x10000;
  dst[0] = 0b1101100000000000 | (code >> 10);
  dst[1] = 0b1101110000000000 | (0b0000001111111111 & code);
  return 4;
}

// NOLINTNEXTLINE
static bool is_code_in_range(uint32_t* range, uint32_t code) {
  for (int i = 0; range[i] != -1; i += 2) {
    if (range[i] <= code && code <= range[i + 1]) {
      return true;
    }
  }
  return false;
}

bool can_be_ident(uint32_t code) {
  // 2n-th code is the start and 2n+1-th code is the end of the ragne.
  static uint32_t range[] = {
    '_',
    '_',
    'a',
    'z',
    'A',
    'Z',
    0x00A8,
    0x00A8,
    0x00AA,
    0x00AA,
    0x00AD,
    0x00AD,
    0x00AF,
    0x00AF,
    0x00B2,
    0x00B5,
    0x00B7,
    0x00BA,
    0x00BC,
    0x00BE,
    0x00C0,
    0x00D6,
    0x00D8,
    0x00F6,
    0x00F8,
    0x00FF,
    0x0100,
    0x02FF,
    0x0370,
    0x167F,
    0x1681,
    0x180D,
    0x180F,
    0x1DBF,
    0x1E00,
    0x1FFF,
    0x200B,
    0x200D,
    0x202A,
    0x202E,
    0x203F,
    0x2040,
    0x2054,
    0x2054,
    0x2060,
    0x206F,
    0x2070,
    0x20CF,
    0x2100,
    0x218F,
    0x2460,
    0x24FF,
    0x2776,
    0x2793,
    0x2C00,
    0x2DFF,
    0x2E80,
    0x2FFF,
    0x3004,
    0x3007,
    0x3021,
    0x302F,
    0x3031,
    0x303F,
    0x3040,
    0xD7FF,
    0xF900,
    0xFD3D,
    0xFD40,
    0xFDCF,
    0xFDF0,
    0xFE1F,
    0xFE30,
    0xFE44,
    0xFE47,
    0xFFFD,
    0x10000,
    0x1FFFD,
    0x20000,
    0x2FFFD,
    0x30000,
    0x3FFFD,
    0x40000,
    0x4FFFD,
    0x50000,
    0x5FFFD,
    0x60000,
    0x6FFFD,
    0x70000,
    0x7FFFD,
    0x80000,
    0x8FFFD,
    0x90000,
    0x9FFFD,
    0xA0000,
    0xAFFFD,
    0xB0000,
    0xBFFFD,
    0xC0000,
    0xCFFFD,
    0xD0000,
    0xDFFFD,
    0xE0000,
    0xEFFFD,
    -1,
  };

  return is_code_in_range(range, code);
}

bool can_be_ident2(uint32_t code) {
  static uint32_t range[] = {
    '0',
    '9',
    0x0300,
    0x036F,
    0x1DC0,
    0x1DFF,
    0x20D0,
    0x20FF,
    0xFE20,
    0xFE2F,
    -1,
  };

  return can_be_ident(code) || is_code_in_range(range, code);
}