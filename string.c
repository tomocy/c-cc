#include "cc.h"

char* format(char* fmt, ...) {
  char* dst = NULL;
  size_t dst_len = 0;
  FILE* out = open_memstream(&dst, &dst_len);

  va_list args;
  va_start(args, fmt);
  vfprintf(out, fmt, args);
  va_end(args);
  fclose(out);
  return dst;
}

bool equal_to_str(char* s, char* other) {
  return strcmp(s, other) == 0;
}

bool equal_to_n_chars(char* s, char* c, int n) {
  return strlen(s) == n && strncmp(s, c, n) == 0;
}

bool start_with(char* s, char* prefix) {
  return strncmp(s, prefix, strlen(prefix)) == 0;
}

bool start_with_insensitive(char* s, char* prefix) {
  return strncasecmp(s, prefix, strlen(prefix)) == 0;
}