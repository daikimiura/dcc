#!/bin/bash -x

TMP=tmp-self

mkdir -p $TMP

# 引数で指定されたファイルをdccでコンパイルしてオブジェクトファイルを上書きする
expand() {
  cat <<EOF >$TMP/$1
typedef struct FILE FILE;
extern FILE *stdout;
extern FILE *stderr;
void *malloc(long size);
void *calloc(long nmemb, long size);
char *strerror(int errnum);
FILE *fopen(char *pathname, char *mode);
long fread(void *ptr, long size, long nmemb, FILE *stream);
int feof(FILE *stream);
static void assert() {}
int strcmp(char *s1, char *s2);
int printf(char *fmt, ...);
int exit(int code);
int memcmp(void *buf1, void *buf2, int n);
int sprintf(char *buf, char *fmt, ...);
int strncasecmp(char *s1, char *s2);
long strlen(char *p);
int strncmp(char *p, char *q);
void *memcpy(char *dst, char *src, long n);
char *strndup(char *p, long n);
int isspace(int c);
int isdigit(int c);
char *strstr(char *haystack, char *needle);
long strtol(char *nptr, char **endptr, int base);
typedef struct {
  int gp_offset;
  int fp_offset;
  void *overflow_arg_area;
  void *reg_save_area;
} __va_elem;
typedef __va_elem va_list[1];
static void va_start(__va_elem *ap) {
  __builtin_va_start(ap);
}
static void va_end(__va_elem *ap) {}
int vfprintf(FILE *stream, char *format, va_list arg);
int fprintf(FILE *stream, char *format, ...);
EOF

  grep -v '^#' dcc.h >>$TMP/$1
  grep -v '^#' $1 >>$TMP/$1

  # \b: 単語の先頭あるいは末尾にマッチ
  # http://www.kt.rim.or.jp/~kbk/regex/regex.html#NBOUNDARY
  gsed -i 's/\bbool\b/_Bool/g' $TMP/$1
  gsed -i 's/\btrue\b/1/g; s/\bfalse\b/0/g;' $TMP/$1
  gsed -i 's/\bNULL\b/0/g' $TMP/$1
  gsed -i 's/INT_MAX/2147483647/g' $TMP/$1

  ./dcc $TMP/$1 >$TMP/${1%.c}.s
  gcc -c -o $TMP/${1%.c}.o $TMP/${1%.c}.s
}

cp *.c $TMP
for i in $TMP/*.c; do
  # `変数%パターン` でパターンと一致した部分を取り除いた残りの部分文字列を返す
  # http://dhythm.blog11.fc2.com/blog-entry-156.html
  gcc -I. -c -o ${i%.c}.o $i
done

expand main.c
expand type.c
expand parse.c
expand codegen.c

gcc -o dcc-gen2 $TMP/*.o
