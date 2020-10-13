#!/bin/bash

set -u

cat << EOF | cc -xc -c -o tmp.o -
int test() { return 5; }
int testadd(int a, int b) { return a + b; }
int testaddmul(int a, int b, int c) { return a + b * c; }
int testsum(int a, int b, int c, int d, int e, int f) { return a + b + c + d + e + f; }
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

assert 0 'main() { 0; }'
assert 42 'main() { 42; }'
assert 21 'main() { 5+20-4; }'
assert 41 'main() {  12 + 34 - 5 ; }'
assert 47 'main() { 5+6*7; }'
assert 8 'main() { 5+6/2; }'
assert 15 'main() { 5*(9-6); }'
assert 4 'main() { (3+5)/2; }'
assert 10 'main() { -10+20; }'
assert 15 'main() { -(-3*+5); }'
assert 0 'main() { 0 == 1; }'
assert 1 'main() { 42==42; }'
assert 1 'main() { 0 != 1; }'
assert 0 'main() { 42!=42; }'
assert 1 'main() { 0<1; }'
assert 0 'main() { 1<1; }'
assert 0 'main() { 2<1; }'
assert 1 'main() { 0<=1; }'
assert 1 'main() { 1<=1; }'
assert 0 'main() { 2<=1; }'
assert 1 'main() { 1>0; }'
assert 0 'main() { 1>1; }'
assert 0 'main() { 1>2; }'
assert 1 'main() { 1>=0; }'
assert 1 'main() { 1>=1; }'
assert 0 'main() { 1>=2; }'
assert 0 'main() { a; }'
assert 0 'main() { z; }'
assert 1 'main() { a=1; }'
assert 41 'main() { z =  12 + 34 - 5 ; }'
assert 3 'main() { 1;2;3; }'
assert 1 'main() { a=1;a; }'
assert 41 'main() { z =  12 + 34 - 5 ;z; }'
assert 2 'main() { foo = 2; }'
assert 5 'main() { foo = 2; bar = foo; bar + 3; }'
assert 8 'main() { return 8; }'
assert 5 'main() { return 5; return 8; }'
assert 3 'main() { returnx = 3; }'
assert 5 'main() { a=1; if (a) 5; }'
assert 4 'main() { a=0; if (a) 5; else 4; }'
assert 5 'main() { i=0; while (i < 5) i = i + 1; }'
assert 3 'main() { i=3; while (0) i = i + 1; }'
assert 5 'main() { for (i=0; i < 5; i = i + 1) i; }'
assert 4 'main() { { {2;} {3;} 4; } }'
assert 1 'main() { { a = 0; b = a; c = b; return a == c; } }'
assert 5 'main() { i=0; while (i < 5) { i = i + 1; } }'
assert 5 'main() { test(); }'
assert 13 'main() { testadd(6, 7); }'
assert 62 'main() { testaddmul(6, 7, 8); }'
assert 21 'main() { testsum(1, 2, 3, 4, 5, 6); }'
assert 3 'three() { return 3; } main() { return three(); }'
assert 3 'add(a, b) { return a + b; } main() { return add(1, 2); }'
assert 3 'a() { c=1; } add(a, b) { return a+b; } main() { return add(1, 2); }'
assert 62 'addmul(a, b, c) { return a + b * c; } main() { return addmul(6, 7, 8); }'
assert 21 'sum(a, b, c, d, e, f) { return a+b+c+d+e+f; } main() { return sum(1, 2, 3, 4, 5, 6); }'

echo "OK"
