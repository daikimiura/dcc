//
// Created by 三浦大輝 on 2020/02/20.
//

#ifndef DCC_DCC_H
#define DCC_DCC_H

#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 入力プログラム
extern char *user_input;

//
// tokenize.c
//

// トークンの種類
typedef enum {
  TK_RESERVED, // 記号
  TK_NUM, // 整数
  TK_EOF, // End Of File
} TokenKind;

// トークン型
typedef struct Token Token;
struct Token {
  TokenKind kind; // トークンの種類
  Token *next; // 次のトークン
  int val; // kindがTK_NUMの場合、その値
  char *str; // トークン文字列
  int len; // トークンの長さ
};

void error(char *fmt, ...);

void error_at(char *loc, char *fmt, ...);

bool consume(char *op);

bool expect(char *op);

int expect_number(void);

bool at_eof(void);

Token *tokenize(char *p);

// 現在着目しているトークン
extern Token *token;

//
// parse.c
//

// 抽象構文木のノードの種類
typedef enum {
  ND_ADD, // +
  ND_SUB, // -
  ND_MUL, // *
  ND_DIV, // /
  ND_EQ, // ==
  ND_NE, // !=
  ND_LT, // <
  ND_LE, // <=
  ND_NUM, // 整数
} NodeKind;

// 抽象構文木のノードの型
typedef struct Node Node;
struct Node {
  NodeKind kind; // ノードの型
  Node *lhs; // 左辺
  Node *rhs; // 右辺
  int val; // kindがND_NUMの場合、その値(整数)
};

Node *expr(void);

//
// codegen.c
//

void codegen(Node *node);

#endif //DCC_DCC_H
