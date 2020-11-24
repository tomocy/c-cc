#include <stdio.h>
#include <stdlib.h>

void assert(char* name, int expected, int actual) {
  if (actual == expected) {
    printf("%s => %d\n", name, expected);
    return;
  }

  fprintf(stderr, "%s => expected %d but got %d\n", name, expected, actual);
  exit(1);
}

#define ASSERT(x, y) assert(#y, x, y)

void ok() {
  printf("OK\n");
  exit(0);
}

int add(int a, int b) { return a + b; }

int addmul(int a, int b, int c) { return a + b * c; }

int sum(int a, int b, int c, int d, int e, int f) {
  return a + b + c + d + e + f;
}

void alloc(int** p, int a, int b, int c, int d) {
  void* vs = calloc(4, 4);
  *(int*)vs = a;
  *(int*)(vs + 4) = b;
  *(int*)(vs + 8) = c;
  *(int*)(vs + 12) = d;
  *p = vs;
}