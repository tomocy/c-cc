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

assert 0 '0;'
assert 42 '42;'
assert 21 '5+20-4;'
assert 41 ' 12 + 34 - 5 ;'
assert 47 '5+6*7;'
assert 8 '5+6/2;'
assert 15 '5*(9-6);'
assert 4 '(3+5)/2;'
assert 10 '-10+20;'
assert 15 '-(-3*+5);'
assert 0 '0 == 1;'
assert 1 '42==42;'
assert 1 '0 != 1;'
assert 0 '42!=42;'
assert 1 '0<1;'
assert 0 '1<1;'
assert 0 '2<1;'
assert 1 '0<=1;'
assert 1 '1<=1;'
assert 0 '2<=1;'
assert 1 '1>0;'
assert 0 '1>1;'
assert 0 '1>2;'
assert 1 '1>=0;'
assert 1 '1>=1;'
assert 0 '1>=2;'
assert 0 'a;'
assert 0 'z;'
assert 1 'a=1;'
assert 41 'z =  12 + 34 - 5 ;'
assert 3 '1;2;3;'
assert 1 'a=1;a;'
assert 41 'z =  12 + 34 - 5 ;z;'
assert 2 'foo = 2;'
assert 5 'foo = 2; bar = foo; bar + 3;'
assert 8 'return 8;'
assert 5 'return 5; return 8;'
assert 3 'returnx = 3;'
assert 5 'a=1; if (a) 5;'
assert 4 'a=0; if (a) 5; else 4;'
assert 5 'i=0; while (i < 5) i = i + 1;'
assert 3 'i=3; while (0) i = i + 1;'
assert 5 'for (i=0; i < 5; i = i + 1) i;'
assert 4 '{ {2;} {3;} 4; }'
assert 1 '{ a = 0; b = a; c = b; return a == c; }'
assert 5 'i=0; while (i < 5) { i = i + 1; }'
assert 5 'test();'
assert 13 'testadd(6, 7);'
assert 62 'testaddmul(6, 7, 8);'
assert 21 'testsum(1, 2, 3, 4, 5, 6);'

echo "OK"
