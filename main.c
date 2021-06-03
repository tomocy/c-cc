#include "cc.h"

char* input_filename;
char* output_filename;

static void usage(int status) {
  fprintf(stderr, "Usage: cc <filename>\n");
  exit(status);
}

static void parse_args(int argc, char** argv) {
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--help") == 0) {
      usage(0);
    }

    if (strcmp(argv[i], "-o") == 0) {
      if (!argv[++i]) {
        usage(1);
      }
      output_filename = argv[i];
      continue;
    }

    if (strncmp(argv[i], "-o", 2) == 0) {
      output_filename = argv[i] + 2;
      continue;
    }

    if (argv[i][0] == '-' && argv[i][1] != '\0') {
      error("unknown argument: %s", argv[i]);
    }

    input_filename = argv[i];
  }

  if (!input_filename) {
    error("no input files");
  }
}

int main(int argc, char** argv) {
  parse_args(argc, argv);
  Token* tokens = tokenize();
  Obj* codes = parse(tokens);
  gen(codes);
  return 0;
}