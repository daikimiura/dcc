//
// Created by 三浦大輝 on 2020/02/20.
//

#include "dcc.h"

// 非終端記号を表す関数のプロトタイプ宣言
Function *program();

Function *function();

Node *stmt();

Node *expr();

Node *assign();

Node *equality();

Node *relational();

Node *add();

Node *mul();

Node *unary();

Node *primary();

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

Node *new_node_return(Node *expr) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = ND_RETURN;
  node->lhs = expr;
  return node;
}

Node *new_node_if() {
  Node *node = calloc(1, sizeof(Node));
  node->kind = ND_IF;
  return node;
}

Node *new_node_while() {
  Node *node = calloc(1, sizeof(Node));
  node->kind = ND_WHILE;
  return node;
}

Node *new_node_for() {
  Node *node = calloc(1, sizeof(Node));
  node->kind = ND_FOR;
  return node;
}

Node *new_node_block() {
  Node *node = calloc(1, sizeof(Node));
  node->kind = ND_BLOCK;
  return node;
}

// funcargs = "(" (assign ("," assign)*)? ")"
Node *funcargs() {
  if (consume(")"))
    return NULL;

  // 連結リストで引数を管理
  Node *head = assign();
  Node *cur = head;
  while (consume(",")) {
    cur->next = assign();
    cur = cur->next;
  }
  expect(")");
  return head;
}

Node *new_node_fun_call(char *funcname) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = ND_FUNCALL;
  node->funcname = funcname;
  node->args = funcargs();
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

// program = function*
Function *program() {
  locals = NULL;

  Function head = {};
  Function *cur = &head;

  while (!at_eof()) {
    cur->next = function();
    cur = cur->next;
  }

  return head.next;
}

// function = ident "(" ")" "{" stmt* "}"
Function *function() {
  locals = NULL;

  char *name = expect_ident();
  expect("(");
  expect(")");
  expect("{");

  // stmtを連結リストで管理
  Node head = {};
  Node *cur = &head;

  while (!consume("}")) {
    cur->next = stmt();
    cur = cur->next;
  }

  Function *fn = calloc(1, sizeof(Function));
  fn->name = name;
  fn->node = head.next;
  fn->locals = locals;
  return fn;
}

// stmt = "return" expr ";"
//      | "if" "(" expr ")" stmt ( "else" stmt )?
//      | "while" "(" expr ")" stmt
//      | "for" "(" expr? ";" expr? ";" expr? ")" stmt
//      | expr ";"
//      | "{" stmt* "}"
Node *stmt() {
  if (consume("return")) {
    Node *node = new_node_return(expr());
    expect(";");
    return node;
  }

  if (consume("if")) {
    Node *node = new_node_if();

    expect("(");
    node->cond = expr();
    expect(")");
    node->then = stmt();

    if (consume("else"))
      node->els = stmt();

    return node;
  }

  if (consume("while")) {
    Node *node = new_node_while();

    expect("(");
    node->cond = expr();
    expect(")");
    node->then = stmt();
    return node;
  }

  if (consume("for")) {
    Node *node = new_node_for();

    expect("(");
    if (!consume(";")) {
      node->init = expr();
      expect(";");
    }

    if (!consume(";")) {
      node->cond = expr();
      expect(";");
    }

    if (!consume(")")) {
      node->inc = expr();
      expect(")");
    }

    node->then = stmt();
    return node;
  }

  if (consume("{")) {
    Node *node = new_node_block();

    // ブロックに含まれるstmtを連結リストで管理
    Node head = {};
    Node *cur = &head;

    while (!consume("}")) {
      cur->next = stmt();
      cur = cur->next;
    }

    node->body = head.next;
    return node;
  }

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

// primary = "(" expr ")" | ident func-args? | num
Node *primary() {
  if (consume("(")) {
    Node *node = expr();
    expect(")");
    return node;
  }

  Token *tok = consume_ident();
  if (tok) {
    if (consume("(")) {
      Node *node = new_node_fun_call(strndup(tok->str, tok->len));
      return node;
    }

    LVar *lvar = find_lvar(tok);
    if (!lvar) {
      lvar = new_lvar(strndup(tok->str, tok->len));
    }
    return new_node_lvar(lvar);
  }

  return new_node_num(expect_number());
}
