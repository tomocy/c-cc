#!/bin/bash

set -u

assert() {
  expected="$1"
  input="$2"

  ./cc "$input" > tmp.s
  cc -o tmp tmp.s
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

echo "OK"
