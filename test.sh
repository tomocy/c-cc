#!/bin/bash

set -u

cat << EOF | cc -xc -c -o tmp.o -
#include <stdlib.h>

int test() { return 5; }
int testadd(int a, int b) { return a + b; }
int testaddmul(int a, int b, int c) { return a + b * c; }
int testsum(int a, int b, int c, int d, int e, int f) { return a + b + c + d + e + f; }
void testalloc(int** p, int a, int b, int c, int d) {
  void* vs = calloc(4, 8);
  *(int*)vs = a;
  *(int*)(vs + 8) = b;
  *(int*)(vs + 16) = c;
  *(int*)(vs + 24) = d;
  *p = vs;
}
EOF

assert() {
  expected="$1"
  input="$2"

  ./cc "$input" > tmp.s
  cc -o tmp tmp.s tmp.o
  ./tmp
  actual="$?"

  if [ "$actual" = "$expected" ]; then
    echo "$input => $expected"
  else
    echo "$input => $expected expected, but got $actual"
    exit 1
  fi
}

assert 0 'int main() { 0; }'
assert 42 'int main() { 42; }'
assert 21 'int main() { 5+20-4; }'
assert 41 'int main() {  12 + 34 - 5 ; }'
assert 47 'int main() { 5+6*7; }'
assert 8 'int main() { 5+6/2; }'
assert 60 'int main() { 3*4*5; }'
assert 2 'int main() { 3*4/6; }'
assert 15 'int main() { 5*(9-6); }'
assert 4 'int main() { (3+5)/2; }'
assert 10 'int main() { -10+20; }'
assert 15 'int main() { -(-3*+5); }'
assert 0 'int main() { 0 == 1; }'
assert 1 'int main() { 42==42; }'
assert 1 'int main() { 0 != 1; }'
assert 0 'int main() { 42!=42; }'
assert 1 'int main() { 0<1; }'
assert 0 'int main() { 1<1; }'
assert 0 'int main() { 2<1; }'
assert 1 'int main() { 0<=1; }'
assert 1 'int main() { 1<=1; }'
assert 0 'int main() { 2<=1; }'
assert 1 'int main() { 1>0; }'
assert 0 'int main() { 1>1; }'
assert 0 'int main() { 1>2; }'
assert 1 'int main() { 1>=0; }'
assert 1 'int main() { 1>=1; }'
assert 0 'int main() { 1>=2; }'
assert 0 'int main() { int a; }'
assert 0 'int main() { int z; }'
assert 1 'int main() { int a; a=1; }'
assert 41 'int main() { int z; z =  12 + 34 - 5 ; }'
assert 3 'int main() { 1;2;3; }'
assert 1 'int main() { int a; a=1; a; }'
assert 41 'int main() { int z;z =  12 + 34 - 5 ;z; }'
assert 2 'int main() { int foo; foo = 2; }'
assert 5 'int main() { int foo; foo = 2; int bar; bar = foo; bar + 3; }'
assert 8 'int main() { return 8; }'
assert 5 'int main() { return 5; return 8; }'
assert 3 'int main() { int returnx; returnx = 3; }'
assert 5 'int main() { int a; a=1; if (a) 5; }'
assert 4 'int main() { int a; a=0; if (a) 5; else 4; }'
assert 5 'int main() { int i; i=0; while (i < 5) i = i + 1; }'
assert 3 'int main() { int i; i=3; while (0) i = i + 1; }'
assert 5 'int main() { int i; for (i=0; i < 5; i = i + 1) i; }'
assert 4 'int main() { { {2;} {3;} 4; } }'
assert 1 'int main() { { int a; a = 0; int b; b = a; int c; c = b; return a == c; } }'
assert 5 'int main() { int i; i=0; while (i < 5) { i = i + 1; } }'
assert 5 'int main() { test(); }'
assert 13 'int main() { testadd(6, 7); }'
assert 62 'int main() { testaddmul(6, 7, 8); }'
assert 21 'int main() { testsum(1, 2, 3, 4, 5, 6); }'
assert 3 'int three() { return 3; } int main() { return three(); }'
assert 3 'int add(int a, int b) { return a + b; } int main() { return add(1, 2); }'
assert 3 'int a() { int c; c=1; } int add(int a, int b) { return a+b; } int main() { return add(1, 2); }'
assert 62 'int addmul(int a, int b, int c) { return a + b * c; } int main() { return addmul(6, 7, 8); }'
assert 21 'int sum(int a, int b, int c, int d, int e, int f) { return a+b+c+d+e+f; } int main() { return sum(1, 2, 3, 4, 5, 6); }'
assert 3 'int main() { int x; x = 3; int y; y = &x; return *y; }'
assert 5 'int main() { int x; x = 3; int y; y = &x; *y = 5; return x; }'
assert 5 'int main() { int x; x = 3; int* y; y = &x; *y = 5; return x; }'
assert 3 'int main() { int x; x = 3; int* y; y = &x; int** z; z = &y; return **z; }'
assert 7 'int* a(int val) { int x; x = val; return &x; } int main() { int* y; y = a(7); return *y; }'
assert 1 'int main() { int* p; testalloc(&p, 1, 2, 4, 8); return *p; }'
assert 2 'int main() { int* p; testalloc(&p, 1, 2, 4, 8); return *(p + 1); }'
assert 4 'int main() { int* p; testalloc(&p, 1, 2, 4, 8); return *(p + 2); }'
assert 8 'int main() { int* p; testalloc(&p, 1, 2, 4, 8); return *(p + 3); }'
assert 2 'int main() { int* p; testalloc(&p, 1, 2, 4, 8); return *(p + 3 - 2); }'
assert 15 'int main() { int* p; testalloc(&p, 1, 2, 4, 8); return *p + *(p + 1) + *(p + 2) + *(p + 3); }'
assert 8 'int main() { return sizeof 1; }'
assert 8 'int main() { int a; return sizeof(a); }'
assert 8 'int main() { int a; return sizeof(&a); }'

echo "OK"
