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

//
// トークナイザー
//

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

    if (*p=='+' || *p =='-' || *p == '*' || *p == '/' || *p == '(' || *p == ')'){
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

//
// パーザー
//

// 抽象構文木のノードの種類
typedef enum {
  ND_ADD, // +
  ND_SUB, // -
  ND_MUL, // *
  ND_DIV, // /
  ND_NUM, // 整数
} NodeKind;

typedef struct Node Node;

// 抽象構文木のノードの型
struct Node {
  NodeKind kind; // ノードの型
  Node *lhs; // 左辺
  Node *rhs; // 右辺
  int val; // kindがND_NUMの場合、その値(整数)
};

Node *new_node(NodeKind kind, Node *lhs, Node *rhs) {
  Node * node = calloc(1, sizeof(Node));
  node->kind = kind;
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

Node *new_node_num(int val) {
  Node * node = calloc(1, sizeof(Node));
  node->kind = ND_NUM;
  node->val = val;
  return node;
}

// 非終端記号を表す関数のプロトタイプ宣言
Node *expr();
Node *mul();
Node *unary();
Node *primary();

// expr = mul ("+" mul | "-" mul)*
Node *expr() {
  Node *node = mul();

  for(;;) {
    if(consume('+'))
      node = new_node(ND_ADD, node, mul());
    else if(consume('-'))
      node = new_node(ND_SUB, node, mul());
    else
      return node;
  }
}

// mul = unary ("*" unary | "/" unary)*
Node *mul() {
  Node *node = unary();
  for(;;) {
    if(consume('*'))
      node = new_node(ND_MUL, node, unary());
    else if (consume('/'))
      node = new_node(ND_DIV, node, unary());
    else
      return node;
  }
}

// unary = ("+" | "-")? primary
Node *unary() {
  if(consume('+'))
    return primary();
  if(consume('-'))
    return new_node(ND_SUB, new_node_num(0), primary());
  return primary();
}

// primary = "(" expr ")" | num
Node *primary() {
  if(consume('(')) {
    Node *node = expr();
    expect(')');
    return node;
  }

  return new_node_num(expect_number());
}

void gen(Node *node){
  if (node->kind == ND_NUM) {
    printf("  push %d\n", node->val);
    return;
  }

  gen(node->lhs);
  gen(node->rhs);

  printf("  pop rdi\n");
  printf("  pop rax\n");

  switch(node->kind) {
    case ND_ADD:
      printf("  add rax, rdi\n");
      break;
    case ND_SUB:
      printf("  sub rax, rdi\n");
      break;
    case ND_MUL:
      printf("  imul rax, rdi\n");
      break;
    case ND_DIV:
      // cqo: RAXに入っている64ビットの値を128ビットに伸ばしてRDXとRAXにセットする
      printf("  cqo\n");
      // idiv: 暗黙のうちにRDXとRAXを取って、それを合わせたものを128ビット整数とみなして、それを引数のレジスタの64ビットの値で割り
      //       商をRAXに、余りをRDXにセットする
      printf("  idiv rdi\n");
      break;
    default:
      error("パーズエラー");
      break;
  }

  printf("  push rax\n");
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "引数の個数が正しくありません\n");
    return 1;
  }

  // トークナイズしてパースする
  user_input = argv[1];
  token = tokenize(user_input);
  Node *node = expr();

  // アセンブリの前半部分を出力
  printf(".intel_syntax noprefix\n");
  printf(".global _main\n");
  printf("_main:\n");

  // 抽象構文木を下りながらコード生成
  gen(node);

  // スタックトップに式(expr)全体の値が残っているはずなので
  // それをRAXにロードして関数からの返り値とする
  printf("  pop rax\n");
  printf("  ret\n");
  return 0;
}
