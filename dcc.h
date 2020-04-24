//
// Created by 三浦大輝 on 2020/02/20.
//

#ifndef DCC_DCC_H
#define DCC_DCC_H

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Type Type;

// 入力ファイル名
extern char *filename;

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
  TK_STR, // 文字列リテラル
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
  char *contents; // 文字列リテラルの内容
  int cont_len; // 文字列リテラルの長さ
};

// 変数
// Varという構造体で一つの変数を表すことにして、先頭の要素をlocals/globalsというポインタで持つ
typedef struct Var Var;
struct Var {
  char *name; // 変数の名前
  Type *ty;
  bool is_local; // ローカル変数 or グローバル変数
  int offset; // RBPからのオフセット
  char *contents; // 変数が文字列リテラルのとき、その値(終端のnullバイト'\0'を含む)
  int cont_len; // 変数が文字列リテラルのとき、その長さ(終端のnullバイト'\0'を含む)
};

// ローカル変数のリスト
// 連結リストで管理する
typedef struct VarList VarList;
struct VarList {
  VarList *next;
  Var *var;
};

void error(char *fmt, ...);

void error_at(char *loc, char *fmt, ...);

bool consume(char *op);

Token *peek(char *op);

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
  ND_ADD, // num + num
  ND_PTR_ADD, // ptr + num, num + ptr
  ND_SUB, // num - num
  ND_PTR_SUB, // ptr - num
  ND_PTR_DIFF, // ptr - ptr
  ND_MUL, // *
  ND_DIV, // /
  ND_EQ, // ==
  ND_NE, // !=
  ND_LT, // <
  ND_LE, // <=
  ND_ASSIGN, // =
  ND_ADDR, // &
  ND_DEREF, // *
  ND_VAR, // ローカル変数 or グローバル変数
  ND_NUM, // 整数
  ND_RETURN, // return
  ND_IF, // if
  ND_WHILE, // while
  ND_FOR, // for
  ND_BLOCK, // ブロック { ... }
  ND_FUNCALL, // 関数呼び出し
  ND_NULL,
} NodeKind;

// 抽象構文木のノードの型
typedef struct Node Node;
struct Node {
  NodeKind kind; // ノードの種類
  Node *next; // 次のノード(';'区切りで複数の式を書く場合 or 関数の引数)
  Type *ty; // ノードの型(Type)
  Node *lhs; // 左辺
  Node *rhs; // 右辺
  Var *var; // kindがND_VARの場合、その変数
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
  VarList *locals; // ローカル変数
  VarList *params; // 引数
  int stack_size; // 引数の個数 * 8 (関数呼び出し時にに下げるスタックの大きさ)
};

typedef struct {
  VarList *globals; // プログラム全体に含まれるグローバル変数
  Function *fns; // プログラム全体に含まれる関数
} Program;

Program *program(void);

//
// codegen.c
//

void codegen(Program *prog);

//
// type.c
//

typedef enum {
  TY_INT,
  TY_CHAR,
  TY_PTR,
  TY_ARRAY
} TypeKind;

struct Type {
  TypeKind kind;
  int size; // sizeof()の値
  Type *ptr_to; // kindがTY_PTRの時、指しているTypeオブジェクトへのポインタ
  int array_len; // kindがTY_ARRAYの時、配列の長さ
};

bool is_integer(Type *ty);

Type *pointer_to(Type *ptr_to);

void add_type(Node *node);

Type *char_type;
Type *int_type;

Type *array_of(Type *pointer_to, int size);

#endif //DCC_DCC_H
