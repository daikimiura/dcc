#!/bin/bash
cat <<EOF | gcc -xc -c -o tmp2.o -
int ret3() {return 3;}
int ret5() {return 5;}

int add(int x, int y) { return x+y; }
int sub(int x, int y) { return x-y; }
int add6(int a, int b, int c, int d, int e, int f) {
  return a+b+c+d+e+f;
}
EOF

try() {
  expected="$1"
  input="$2"

  ./dcc "$input" >tmp.s
  gcc -o tmp tmp.s tmp2.o
  ./tmp
  actual="$?"

  if [ "$actual" = "$expected" ]; then
    echo "$input => $actual"
  else
    echo "$input => $expected expected, but got $actual"
    exit 1
  fi
}

try 42 'return 42;'
try 3 'return 1+2;'
try 21 'return 5+20-4;'
try 29 'return 20 + 20 + 2 - 13 ;'
try 7 'return 1+2*3;'
try 12 'return 4*(5-2);'
try 2 'return (1+5)/3;'
try 10 'return -10+20;'
try 9 'return -3*-3;'
try 1 'return 1==1;'
try 0 'return 0==1;'
try 1 'return 0!=1;'
try 0 'return 42!=42;'
try 1 'return 0<1;'
try 0 'return 1<1;'
try 1 'return 0<=1;'
try 1 'return 1<=1;'
try 0 'return 1<=0;'
try 1 'return 1>0;'
try 0 'return 1>1;'
try 1 'return 1>=0;'
try 1 'return 1>=1;'
try 0 'return 1>=2;'
try 5 '1+1;return 5;'
try 2 'a=2;return a;'
try 8 'a=3;b=5;return a+b;'
try 14 'a=1+2;b=4;return (a+b)*2;'
try 3 'a=b=c=1;return a+b+c;'
try 6 'foo=6;return foo;'
try 11 'foo=2;bar=9;return foo+bar;'
try 13 'foo1=3;bar2=5;return foo1+bar2+5;'
try 3 'if (1==2) return 2; return 3;'
try 2 'if (1) return 2; return 3;'
try 2 'if (1==1) return 2; return 3;'
try 3 'i=0;while(i<3)i=i+1;return i;'
try 1 'i=1;while(i>2)i=i+1;return i;'
try 55 'i=0; j=0; for (i=0; i<=10; i=i+1) j=i+j; return j;'
try 3 'i=2; for(;i>3;i=i+1) return 2; return 3;'
try 2 'for (;;) return 2; return 3;'
try 2 '{i = 1; i= 1+1; return i;}'
try 4 'i=1;j=2;if(i<2) {i=i+1; j=j+2;} return j;'
try 3 'return ret3();'
try 5 'return ret5();'
try 8 'return add(3, 5);'
try 2 'return sub(5, 3);'
try 21 'return add6(1,2,3,4,5,6);'

echo ok
