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

try 42 'int main() { return 42;}'
try 3 'int main() { return 1+2;}'
try 21 'int main() { return 5+20-4;}'
try 29 'int main() { return 20 + 20 + 2 - 13 ;}'
try 7 'int main() { return 1+2*3;}'
try 12 'int main() { return 4*(5-2);}'
try 2 'int main() { return (1+5)/3;}'
try 10 'int main() { return -10+20;}'
try 9 'int main() { return -3*-3;}'
try 1 'int main() { return 1==1;}'
try 0 'int main() { return 0==1;}'
try 1 'int main() { return 0!=1;}'
try 0 'int main() { return 42!=42;}'
try 1 'int main() { return 0<1;}'
try 0 'int main() { return 1<1;}'
try 1 'int main() { return 0<=1;}'
try 1 'int main() { return 1<=1;}'
try 0 'int main() { return 1<=0;}'
try 1 'int main() { return 1>0;}'
try 0 'int main() { return 1>1;}'
try 1 'int main() { return 1>=0;}'
try 1 'int main() { return 1>=1;}'
try 0 'int main() { return 1>=2;}'
try 5 'int main() {1+1;return 5;}'
try 2 'int main() {int a=2;return a;}'
try 8 'int main() {int a=3; int b=5;return a+b;}'
try 14 'int main() {int a=1+2; int b=4;return (a+b)*2;}'
try 3 'int main() {int a=1;int b=1;int c=1;return a+b+c;}'
try 6 'int main() {int foo=6;return foo;}'
try 11 'int main() {int foo=2;int bar=9;return foo+bar;}'
try 13 'int main() {int foo1=3;int bar2=5;return foo1+bar2+5;}'
try 3 'int main() {if (1==2) return 2; return 3;}'
try 2 'int main() {if (1) return 2; return 3;}'
try 2 'int main() {if (1==1) return 2; return 3;}'
try 3 'int main() {int i=0;while(i<3)i=i+1;return i;}'
try 1 'int main() {int i=1;while(i>2)i=i+1;return i;}'
try 55 'int main() {int i=0; int j=0; for (i=0; i<=10; i=i+1) j=i+j; return j;}'
try 3 'int main() {int i=2; for(;i>3;i=i+1) return 2; return 3;}'
try 2 'int main() {for (;;) return 2; return 3;}'
try 2 'int main() {int i = 1; i= 1+1; return i;}'
try 4 'int main() {int i=1; int j=2;if(i<2) {i=i+1; j=j+2;} return j;}'
try 3 'int main() { return ret3();}'
try 5 'int main() { return ret5();}'
try 8 'int main() { return add(3, 5);}'
try 2 'int main() { return sub(5, 3);}'
try 21 'int main() { return add6(1,2,3,4,5,6);}'
try 5 'int foo(){return 5;} int main() { return foo();}'
try 8 'int main() { return fib(5); } int fib(int x) { if(x<2){return 1;}else{return fib(x-1) + fib(x-2);} }'
try 3 'int main() { int x=3; return *&x; }'
try 3 'int main() { int x=3; int *y=&x; int **z=&y; return x; }'
try 5 'int main() { int x=3; int y=5; return *(&x+1); }'
try 3 'int main() { int x=3; int y=5; return *(&y-1); }'
try 5 'int main() { int x=3; int *y=&x; *y=5; return x; }'
try 7 'int main() { int x=3; int y=5; *(&x+1)=7; return y; }'
try 7 'int main() { int x=3; int y=5; *(&y-1)=7; return x; }'
try 5 'int main() { int x=3; return (&x+5)-&x; }'
try 3 'int main() { int x[2]; int *y=&x; *y=3; return *x; }'
try 0 'int main() { int x[2][3]; int *y=x; *y=0; return **x; }'
try 1 'int main() { int x[2][3]; int *y=x; *(y+1)=1; return *(*x+1); }'
try 2 'int main() { int x[2][3]; int *y=x; *(y+2)=2; return *(*x+2); }'
try 3 'int main() { int x[2][3]; int *y=x; *(y+3)=3; return **(x+1); }'
try 4 'int main() { int x[2][3]; int *y=x; *(y+4)=4; return *(*(x+1)+1); }'
# 宣言したメモリの範囲外にもアクセスできる
try 5 'int main() { int x[2][3]; int *y=x; *(y+5)=5; return *(*(x+1)+2); }'

echo ok
