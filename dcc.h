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
  TK_IDENT, // 識別子
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

// ローカル変数型
// LVarという構造体で一つの変数を表すことにして、先頭の要素をlocalsというポインタで持つ
typedef struct LVar LVar;
struct LVar {
  char *name; // 変数の名前
  int offset; // RBPからのオフセット
};

// ローカル変数のリスト
// 連結リストで管理する
typedef struct LVarList LVarList;
struct LVarList {
  LVarList *next;
  LVar *lvar;
};

void error(char *fmt, ...);

void error_at(char *loc, char *fmt, ...);

bool consume(char *op);

Token *consume_ident(void);

bool expect(char *op);

int expect_number(void);

char *expect_ident(void);

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
  ND_ASSIGN, // =
  ND_ADDR, // &
  ND_DEREF, // *
  ND_LVAR, // ローカル変数
  ND_NUM, // 整数
  ND_RETURN, // return
  ND_IF, // if
  ND_WHILE, // while
  ND_FOR, // for
  ND_BLOCK, // ブロック { ... }
  ND_FUNCALL, // 関数呼び出し
} NodeKind;

// 抽象構文木のノードの型
typedef struct Node Node;
struct Node {
  NodeKind kind; // ノードの型
  Node *next; // 次のノード(';'区切りで複数の式を書く場合 or 関数の引数)
  Node *lhs; // 左辺
  Node *rhs; // 右辺
  LVar *lvar; // kindがND_LVARの場合、その変数
  int val; // kindがND_NUMの場合、その値

  // 制御構文
  Node *cond; // kindがND_IF/ND_WHILE/ND_FORの場合、その条件式
  Node *then; // kindがND_IF/ND_WHILE/ND_FORの場合、条件がtrueの時に評価される式
  Node *els; // kindがND_IFの場合、条件がfalseの時に評価される式
  Node *init; // kindがND_FORの場合、初期値
  Node *inc; // kindがND_FORの場合、ループごとの増分

  // ブロック
  Node *body; // kindがND_BLOCKの時、ブロックに含まれる式

  // 関数
  char *funcname; // kindがND_FUNCALLの時、関数名
  Node *args; // kindがND_FUNCALLの時、引数
};

typedef struct Function Function;
struct Function {
  Function *next; // 次に実行する関数を連結リストで管理
  char *name; // 関数名
  Node *node; // 関数のブロック部分(実際の処理)
  LVarList *locals; // ローカル変数
  LVarList *params; // 引数
  int stack_size; // 引数の個数 * 8 (関数呼び出し時にに下げるスタックの大きさ)
};

Function *program(void);

//
// codegen.c
//

void codegen(Function *prog);

#endif //DCC_DCC_H
