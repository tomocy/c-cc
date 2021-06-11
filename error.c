#include "cc.h"

void error(char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  va_end(args);
  exit(1);
}

// NOLINTNEXTLINE
static char* bol(char* contents, char* c) {
  char* begin = c;
  while (contents < begin && begin[-1] != '\n') {
    begin--;
  }
  return begin;
}

// NOLINTNEXTLINE
static char* eol(char* contents, char* c) {
  char* end = c;
  while (*end && *end != '\n') {
    end++;
  }
  return end;
}

// NOLINTNEXTLINE
static int count_lines(char* contents, char* loc) {
  int line = 1;
  for (char* c = contents; *c && c < loc; c++) {
    if (*c == '\n') {
      line++;
    }
  }
  return line;
}

static void print_loc(char* fname, char* contents, char* loc) {
  int line = count_lines(contents, loc);
  char* begin = bol(contents, loc);
  char* end = eol(contents, loc);

  int indent = fprintf(stderr, "%s:%d:%d ", fname, line, (int)(loc - begin) + 1);
  fprintf(stderr, "%.*s\n", (int)(end - begin), begin);

  int pos = loc - begin + indent;
  fprintf(stderr, "%*s", pos, "");
  fprintf(stderr, "^ ");
}

// NOLINTNEXTLINE
void vprint_at(char* fname, char* contents, char* loc, char* fmt, va_list args) {
  print_loc(fname, contents, loc);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
}

// NOLINTNEXTLINE
void warn_at(char* fname, char* contents, char* loc, char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vprint_at(fname, contents, loc, fmt, args);
  va_end(args);
}

// NOLINTNEXTLINE
void error_at(char* fname, char* contents, char* loc, char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vprint_at(fname, contents, loc, fmt, args);
  va_end(args);
  exit(1);
}