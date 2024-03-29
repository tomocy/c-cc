#include "cc.h"

void add_str(Str** strs, Str* str) {
  if (!str) {
    return;
  }
  str->next = *strs;
  *strs = str;
}

void push_str(StrQueue* queue, Str* str) {
  if (!queue->cur) {
    queue->cur = &queue->head;
  }
  queue->cur = queue->cur->next = str;
}

void push_strs(StrQueue* queue, Str* strs) {
  for (Str* str = strs; str; str = str->next) {
    push_str(queue, str);
  }
}

Str* new_str(char* data) {
  Str* str = calloc(1, sizeof(Str));
  str->data = data;
  return str;
}

Str* copy_str(Str* src) {
  Str* str = calloc(1, sizeof(Str));
  *str = *src;
  str->next = NULL;
  return str;
}

Str* append_strs(Str* former, Str* latter) {
  Str head = {};
  Str* cur = &head;
  for (Str* str = former; str; str = str->next) {
    cur = cur->next = copy_str(str);
  }
  cur->next = latter;

  return head.next;
}

bool contain_str(Str* strs, char* c, int len) {
  for (Str* str = strs; str; str = str->next) {
    if (equal_to_n_chars(str->data, c, len)) {
      return true;
    }
  }
  return false;
}

Str* intersect_strs(Str* a, Str* b) {
  Str head = {};
  Str* cur = &head;
  for (Str* s = b; s; s = s->next) {
    if (contain_str(a, s->data, strlen(s->data))) {
      cur = cur->next = copy_str(s);
    }
  }

  return head.next;
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

bool start_with_any(char* s, ...) {
  va_list prefixs;
  va_start(prefixs, s);
  for (char* prefix = va_arg(prefixs, char*); prefix; prefix = va_arg(prefixs, char*)) {
    if (start_with(s, prefix)) {
      return true;
    }
  }

  return false;
}

bool start_with_insensitive(char* s, char* prefix) {
  return strncasecmp(s, prefix, strlen(prefix)) == 0;
}

bool end_with(char* s, char* suffix) {
  int len = strlen(suffix);
  return strncmp(s + strlen(s) - len, suffix, len) == 0;
}