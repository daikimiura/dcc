//
// Created by 三浦大輝 on 2020/02/12.
//

#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 入力プログラム
char *user_input;

// トークンの種類
typedef enum {
  TK_RESERVED, // 記号
  TK_NUM, // 整数
  TK_EOF, // End Of File
} TokenKind;

typedef struct Token Token;

// トークン型
struct Token {
  TokenKind kind; // トークンの種類
  Token *next; // 次のトークン
  int val; // kindがTK_NUMの場合、その値
  char *str; // トークン文字列
};

// 現在着目しているトークン
Token *token;

// エラーを報告するための関数
// printfと同じ引数をとる
void error(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

// エラー内容とエラーが起きた位置を報告するための関数
void error_at(char *loc, char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  int pos = loc - user_input;
  fprintf(stderr, "'%s'\n", user_input);
  fprintf(stderr, "%*s", pos, ""); // pos個の空白を出力
  fprintf(stderr, "^ ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);

}

// 現在のトークンが期待している記号の時には、トークンを1つ読み進めてtrueを返す
// それ以外の場合にはfalseを返す
bool consume(char op) {
  if (token->kind != TK_RESERVED || token->str[0] != op)
    return false;
  token = token->next;
  return true;
}

// 現在のトークンが期待している記号の時には、トークンを1つ読み進めてtrueを返す
// それ以外の場合はエラーを返す
bool expect(char op) {
  if (token->kind != TK_RESERVED || token->str[0] != op)
    error_at(token->next->str, "'%c'ではありません", op);
  token = token->next;
  return true;
}

// 現在のトークンが数値の場合、トークンを一つ読み進めてその数値を返す
// それ以外の場合はエラーを返す
int expect_number() {
  if (token->kind != TK_NUM)
    error_at(token->next->str, "数値ではありません");
  int val = token->val;
  token = token->next;
  return val;
}

bool at_eof() {
  return token->kind == TK_EOF;
}

// 新しいトークンを作成してcurに繋げる
Token *new_token(TokenKind kind, Token *cur, char *str) {
  Token *tok = calloc(1, sizeof(Token));
  tok->kind = kind;
  tok->str = str;
  cur->next = tok;
  return tok;
}

// 入力文字列pをトークナイズしてそれを返す
Token *tokenize(char *p){
  Token head;
  head.next = NULL;
  Token *cur = &head;

  while(*p) {
    // 空白文字をスキップ
    if (isspace(*p)){
      p++;
      continue;
    }

    if (*p=='+' || *p =='-'){
      cur = new_token(TK_RESERVED, cur, p++);
      continue;
    }

    if(isdigit(*p)){
      cur = new_token(TK_NUM, cur, p);
      cur->val = strtol(p, &p, 10);
      continue;
    }

    error("トークナイズできません");
  }

  new_token(TK_EOF, cur, p);
  return head.next;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "引数の個数が正しくありません\n");
    return 1;
  }

  user_input = argv[1];
  token = tokenize(argv[1]);

  printf(".intel_syntax noprefix\n");
  printf(".global _main\n");
  printf("_main:\n");

  // 式の最初は数でなければならないので、それをチェックして最初のmov命令を出力
  printf("  mov rax, %d\n", expect_number());

  // `+ <数>` あるいは `- <数>` というトークンの並びを消費しつつアセンブリを出力
  while (!at_eof()) {
    if (consume('+')) {
      printf("  add rax, %d\n", expect_number());
      continue;
    }

    expect('-');
    printf("  sub rax, %d\n", expect_number());
  }

  printf("  ret\n");
  return 0;
}
