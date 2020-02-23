#!/bin/bash
try() {
  expected="$1"
  input="$2"

  ./dcc "$input" >tmp.s
  gcc -o tmp tmp.s
  ./tmp
  actual="$?"

  if [ "$actual" = "$expected" ]; then
    echo "$input => $actual"
  else
    echo "$input => $expected expected, but got $actual"
    exit 1
  fi
}

try 42 '42;'
try 3 '1+2;'
try 21 '5+20-4;'
try 29 ' 20 + 20 + 2 - 13 ;'
try 7 '1+2*3;'
try 12 '4*(5-2);'
try 2 '(1+5)/3;'
try 10 '-10+20;'
try 9 '-3*-3;'
try 1 '1==1;'
try 0 '0==1;'
try 1 '0!=1;'
try 0 '42!=42;'
try 1 '0<1;'
try 0 '1<1;'
try 1 '0<=1;'
try 1 '1<=1;'
try 0 '1<=0;'
try 1 '1>0;'
try 0 '1>1;'
try 1 '1>=0;'
try 1 '1>=1;'
try 0 '1>=2;'
try 5 '1+1;5;'
try 2 'a=2;a;'
try 8 'a=3;b=5;a+b;'
try 14 'a=1+2;b=4;(a+b)*2;'
try 3 'a=b=c=1;a+b+c;'
try 6 'foo=6;foo;'
try 11 'foo=2;bar=9;foo+bar;'
try 13 'foo1=3;bar2=5;foo1+bar2+5;'

echo ok
