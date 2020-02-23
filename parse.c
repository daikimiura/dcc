//
// Created by 三浦大輝 on 2020/02/20.
//

#include "dcc.h"

Node *new_node(NodeKind kind, Node *lhs, Node *rhs) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

Node *new_node_num(int val) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = ND_NUM;
  node->val = val;
  return node;
}

Node *new_node_lvar(LVar *var) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = ND_LVAR;
  node->lvar = var;
  return node;
}

// 変数を名前で検索する
// 見つからなかった場合はNULLを返す
LVar *find_lvar(Token *tok) {
  for (LVar *var = locals; var; var = var->next) {
    if (strlen(var->name) == tok->len && !strncmp(tok->str, var->name, tok->len)) {
      return var;
    }
  }
  return NULL;
}

// 新しいローカル変数を連結リストの先頭に追加する
LVar *new_lvar(char *name) {
//  printf("new lvar name %s\n", strndup(name, 1));
  LVar *var = calloc(1, sizeof(LVar));
  var->next = locals;
  var->name = name;
  locals = var;
  return var;
}

// 非終端記号を表す関数のプロトタイプ宣言
Node *stmt();

Node *expr();

Node *assign();

Node *equality();

Node *relational();

Node *add();

Node *mul();

Node *unary();

Node *primary();

// program = stmt*
Node *program() {
  locals = NULL;

  Node head = {};
  Node *cur = &head;

  while (!at_eof()) {
    cur->next = stmt();
    cur = cur->next;
  }

  return head.next;
}

// stmt = expr ";"
Node *stmt() {
  Node *node = expr();
  expect(";");
  return node;
}

// expr = assign
Node *expr() {
  return assign();
}

// assign = equality ("=" assign)?
Node *assign() {
  Node *node = equality();

  if (consume("="))
    node = new_node(ND_ASSIGN, node, assign());
  return node;
}

// equality = relational ("==" relational | "!=" relational )*
Node *equality() {
  Node *node = relational();

  for (;;) {
    if (consume("=="))
      node = new_node(ND_EQ, node, relational());
    else if (consume("!="))
      node = new_node(ND_NE, node, relational());
    else
      return node;
  }
}

// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
Node *relational() {
  Node *node = add();

  for (;;) {
    if (consume("<"))
      node = new_node(ND_LT, node, add());
    else if (consume("<="))
      node = new_node(ND_LE, node, add());
    else if (consume(">"))
      node = new_node(ND_LT, add(), node);
    else if (consume(">="))
      node = new_node(ND_LE, add(), node);
    else
      return node;
  }
}

// add = mul ("+" mul | "-" mul)*
Node *add() {
  Node *node = mul();

  for (;;) {
    if (consume("+"))
      node = new_node(ND_ADD, node, mul());
    else if (consume("-"))
      node = new_node(ND_SUB, node, mul());
    else
      return node;
  }
}

// mul = unary ("*" unary | "/" unary)*
Node *mul() {
  Node *node = unary();

  for (;;) {
    if (consume("*"))
      node = new_node(ND_MUL, node, unary());
    else if (consume("/"))
      node = new_node(ND_DIV, node, unary());
    else
      return node;
  }
}

// unary = ("+" | "-")? primary
Node *unary() {
  if (consume("+"))
    return primary();
  if (consume("-"))
    return new_node(ND_SUB, new_node_num(0), primary());
  return primary();
}

// primary = "(" expr ")" | ident | num
Node *primary() {
  if (consume("(")) {
    Node *node = expr();
    expect(")");
    return node;
  }

  Token *tok = consume_ident();
  if (tok) {
    LVar *lvar = find_lvar(tok);
    if (!lvar) {
      lvar = new_lvar(strndup(tok->str, tok->len));
    }
    return new_node_lvar(lvar);
  }

  return new_node_num(expect_number());
}
