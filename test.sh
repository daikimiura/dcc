#!/bin/bash
try() {
  expected="$1"
  input="$2"

  ./dcc "$input" > tmp.s
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

try 42 42
try 21 "5+20-4"
try 29 " 20 + 20 + 2 - 13 "
try 7 "1+2*3"
try 12 "4*(5-2)"
try 2 "(1+5)/3"

echo ok
