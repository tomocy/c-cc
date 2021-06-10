#include "cc.h"

static FILE* open_file(char* fname, char* mode) {
  FILE* file = fopen(fname, mode);
  if (!file) {
    error("cannot open file: %s: %s", fname, strerror(errno));
  }
  return file;
}

FILE* open_input_file(char* fname) {
  if (equal_to_str(fname, "-")) {
    return stdin;
  }
  return open_file(fname, "r");
}

FILE* open_output_file(char* fname) {
  if (!fname || equal_to_str(fname, "-")) {
    return stdout;
  }
  return open_file(fname, "w");
}

char* read_file_contents(char* fname) {
  FILE* file = open_input_file(fname);

  char* contents;
  size_t len;
  FILE* stream = open_memstream(&contents, &len);

  for (;;) {
    char tmp[4096];
    int n = fread(tmp, 1, sizeof(tmp), file);
    if (n == 0) {
      break;
    }
    fwrite(tmp, 1, n, stream);
  }

  if (file != stdin) {
    fclose(file);
  }

  fflush(stream);

  if (len == 0 || contents[len - 1] != '\n') {
    fputc('\n', stream);
  }
  fputc('\0', stream);
  fclose(stream);

  return contents;
}