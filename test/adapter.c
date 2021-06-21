#pragma GCC diagnostic ignored "-Wunused-function"

#include <stdarg.h>
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

int add(int a, int b) {
  return a + b;
}

int addmul(int a, int b, int c) {
  return a + b * c;
}

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

static int static_fn() {
  return 5;
}

int ext1 = 5;
int* ext2 = &ext1;

int ext3 = 7;
int ext_fn1(int x) {
  return x;
}
int ext_fn2(int x) {
  return x;
}

int false_fn() {
  return 512;
}
int true_fn() {
  return 513;
}
int char_fn() {
  return (2 << 8) + 3;
}
int short_fn() {
  return (2 << 16) + 5;
}
int uchar_fn() {
  return (2 << 10) - 1 - 4;
}
int ushort_fn() {
  return (2 << 20) - 1 - 7;
}
int schar_fn() {
  return (2 << 10) - 1 - 4;
}
int sshort_fn() {
  return (2 << 20) - 1 - 7;
}

int add_all(int n, ...) {
  va_list ap;
  va_start(ap, n);

  int sum = 0;
  for (int i = 0; i < n; i++) {
    sum += va_arg(ap, int);
  }
  return sum;
}

float add_float(float x, float y) {
  return x + y;
}

double add_double(double x, double y) {
  return x + y;
}

int add10_int(int x1, int x2, int x3, int x4, int x5, int x6, int x7, int x8, int x9, int x10) {
  return x1 + x2 + x3 + x4 + x5 + x6 + x7 + x8 + x9 + x10;
}

float add10_float(float x1, float x2, float x3, float x4, float x5, float x6, float x7, float x8, float x9, float x10) {
  return x1 + x2 + x3 + x4 + x5 + x6 + x7 + x8 + x9 + x10;
}

double add10_double(double x1,
  double x2,
  double x3,
  double x4,
  double x5,
  double x6,
  double x7,
  double x8,
  double x9,
  double x10) {
  return x1 + x2 + x3 + x4 + x5 + x6 + x7 + x8 + x9 + x10;
}