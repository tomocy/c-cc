#include "cc.h"

int main(int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr, "Usage: cc <code>\n");
    return 1;
  }

  user_input = argv[1];
  tokenize();
  program();
  gen_program();
  return 0;
}