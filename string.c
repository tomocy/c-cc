#include "cc.h"

void add_str(Str** strs, Str* str) {
  str->next = *strs;
  *strs = str;
}

Str* new_str(char* data) {
  Str* str = calloc(1, sizeof(Str));
  str->data = data;
  return str;
}

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