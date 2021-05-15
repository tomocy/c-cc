#pragma GCC diagnostic ignored "-Wunused-function"

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

void ok(void) {
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

static int static_fn() { return 5; }

int ext1 = 5;
int* ext2 = &ext1;

int ext3 = 7;
int ext_fn1(int x) { return x; }
int ext_fn2(int x) { return x; }

int false_fn() { return 512; }
int true_fn() { return 513; }
int char_fn() { return (2 << 8) + 3; }
int short_fn() { return (2 << 16) + 5; }