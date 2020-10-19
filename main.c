#include "cc.h"

int main(int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr, "Usage: cc <filename>\n");
    return 1;
  }

  filename = argv[1];
  tokenize();
  program();
  gen_program();
  return 0;
}