#include "cc.h"

int main(int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr, "Usage: cc <code>\n");
    return 1;
  }

  user_input = argv[1];
  token = tokenize();
  program();

  printf(".intel_syntax noprefix\n");
  printf(".global main\n");
  printf("main:\n");

  printf("  push rbp\n");
  printf("  mov rbp, rsp\n");
  printf("  sub rsp, %d\n", 8 * 26);

  for (int i = 0; stmts[i]; i++) {
    gen(stmts[i]);
    printf("  pop rax\n");
  }

  printf("  mov rsp, rbp\n");
  printf("  pop rbp\n");

  printf("  ret\n");
  return 0;
}