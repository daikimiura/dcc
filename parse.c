//
// Created by 三浦大輝 on 2020/02/20.
//

#include "dcc.h"

VarList *locals;
VarList *globals;

// 非終端記号を表す関数のプロトタイプ宣言
Program *program();

Function *function();

Node *declaration();

Type *basetype();

Node *stmt();

Node *stmt2();

Node *expr();

Node *assign();

Node *equality();

Node *relational();

Node *add();

Node *mul();

Node *unary();

Node *postfix();

Node *primary();

void global_var();

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

Node *new_node_unary(NodeKind kind, Node *expr) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  node->lhs = expr;
  return node;
}

Node *new_node_var(Var *var) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = ND_VAR;
  node->var = var;
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

Node *new_node_add(Node *lhs, Node *rhs) {
  add_type(lhs);
  add_type(rhs);
  if (is_integer(lhs->ty) && is_integer(rhs->ty)) // num + num
    return new_node(ND_ADD, lhs, rhs);
  if (lhs->ty->ptr_to && is_integer(rhs->ty)) // ptr + num
    return new_node(ND_PTR_ADD, lhs, rhs);
  if (is_integer(lhs->ty) && rhs->ty->ptr_to) // num + ptr
    return new_node(ND_PTR_ADD, rhs, lhs);
  error("`+` の左辺値と右辺値のどちらか、もしくは両方が不適です");
}

Node *new_node_sub(Node *lhs, Node *rhs) {
  add_type(lhs);
  add_type(rhs);
  if (is_integer(lhs->ty) && is_integer(rhs->ty)) // num - num
    return new_node(ND_SUB, lhs, rhs);
  if (lhs->ty->ptr_to && is_integer(rhs->ty)) // ptr - num
    return new_node(ND_PTR_SUB, lhs, rhs);
  if (lhs->ty->ptr_to && rhs->ty->ptr_to) // ptr - ptr
    return new_node(ND_PTR_DIFF, lhs, rhs);
  error("`-` の左辺値と右辺値のどちらか、もしくは両方が不適です");
}

Node *new_node_null() {
  Node *node = calloc(1, sizeof(Node));
  node->kind = ND_NULL;
  return node;
}

Node *new_node_stmt_expr() {
  Node *node = calloc(1, sizeof(Node));
  node->kind = ND_STMT_EXPR;
  return node;
}

// .dataセクションのラベルを作成する
// 文字列をグローバル変数として扱うために、普通のグローバル変数とは名前が被らない一意なラベルを用いる
char *new_label() {
  static int cnt = 0;
  char buf[20];
  sprintf(buf, ".L.data.%d", cnt++);
  return strndup(buf, 20);
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

Var *find_lvar(Token *tok) {
  for (VarList *vl = locals; vl; vl = vl->next) {
    Var *lvar = vl->var;
    if (strlen(lvar->name) == tok->len && !strncmp(tok->str, lvar->name, tok->len)) {
      return lvar;
    }
  }
  return NULL;
}

Var *find_gvar(Token *tok) {
  for (VarList *vl = globals; vl; vl = vl->next) {
    Var *gvar = vl->var;
    if (strlen(gvar->name) == tok->len && !strncmp(tok->str, gvar->name, tok->len)) {
      return gvar;
    }
  }
  return NULL;
}


// 変数を名前で検索する
// 見つからなかった場合はNULLを返す
Var *find_var(Token *tok) {
  Var *lvar = find_lvar(tok);
  if (lvar)
    return lvar;

  return find_gvar(tok);
}

// 新しいローカル変数 or グローバル変数 を追加する
Var *new_var(char *name, Type *ty, bool is_local) {
  Var *var = calloc(1, sizeof(Var));
  var->name = name;
  var->ty = ty;
  var->is_local = is_local;
  return var;
}

// 新しいローカル変数(Var)を連結リスト(VarListのリスト)の先頭に追加する
Var *new_lvar(char *name, Type *ty) {
  Var *lvar = new_var(name, ty, true);
  VarList *vl = calloc(1, sizeof(VarList));
  vl->var = lvar;
  vl->next = locals;
  locals = vl;
  return lvar;
}

Var *new_gvar(char *name, Type *ty) {
  Var *gvar = new_var(name, ty, false);
  VarList *vl = calloc(1, sizeof(VarList));
  vl->var = gvar;
  vl->next = locals;
  globals = vl;
  return gvar;
}

// 配列宣言時の型のsuffix(配列の要素数)を読み取る
Type *read_type_suffix(Type *ptr_to) {
  if (!consume("["))
    return ptr_to;
  int size = expect_number();
  expect("]");
  ptr_to = read_type_suffix(ptr_to);
  return array_of(ptr_to, size);
}

VarList *read_func_param() {
  Type *ty = basetype();
  char *name = expect_ident();
  VarList *vl = calloc(1, sizeof(VarList));
  vl->var = new_lvar(name, ty);
  return vl;
}

VarList *read_func_params() {
  if (consume(")"))
    return NULL;

  VarList *head = read_func_param();
  VarList *cur = head;

  while (!consume(")")) {
    expect(",");
    cur->next = read_func_param();
    cur = cur->next;
  }

  return head;
}

bool is_typename() {
  return peek("char") || peek("int");
}

// program() が function() かどうか判定する
bool is_function() {
  Token *tok = token;
  // tokenを先読みして判定
  // basetype ident ( ... ならfunc
  basetype();
  bool is_func = consume_ident() && consume("(");
  token = tok;
  return is_func;
};

// program = (global-var | function)*
Program *program() {
  locals = NULL;
  globals = NULL;

  Function head = {};
  Function *cur = &head;

  while (!at_eof()) {
    if (is_function()) {
      cur->next = function();
      cur = cur->next;
    } else {
      global_var();
    }
  }

  Program *prog = calloc(1, sizeof(Program));
  prog->globals = globals;
  prog->fns = head.next;

  return prog;
}

// global-var = basetype ident ("[" num "]")* ";"
void global_var() {
  Type *ty = basetype();
  char *name = expect_ident();
  ty = read_type_suffix(ty);
  expect(";");
  new_gvar(name, ty);
}

// function = basetype ident "(" params? ")" "{" stmt* "}"
// params = param ("," param)*
// param = basetype ident
Function *function() {
  locals = NULL;

  Function *fn = calloc(1, sizeof(Function));

  basetype();
  fn->name = expect_ident();
  expect("(");
  fn->params = read_func_params();
  expect("{");

  // stmtを連結リストで管理
  Node head = {};
  Node *cur = &head;

  while (!consume("}")) {
    cur->next = stmt();
    cur = cur->next;
  }

  fn->node = head.next;
  fn->locals = locals;
  return fn;
}

// 現時点ではint型/char型/ポインタ型のみ
// basetype = ( "int" | "char" ) "*"*
Type *basetype() {
  Type *ty;
  if (consume("char")) {
    ty = char_type;
  } else {
    expect("int");
    ty = int_type;
  }


  while (consume("*")) {
    ty = pointer_to(ty);
  }
  return ty;
}

// stmtの中身をトラバースして型をつける
Node *stmt(void) {
  Node *node = stmt2();
  add_type(node);
  return node;
}

// stmt = "return" expr ";"
//      | "if" "(" expr ")" stmt ( "else" stmt )?
//      | "while" "(" expr ")" stmt
//      | "for" "(" expr? ";" expr? ";" expr? ")" stmt
//      | expr ";"
//      | "{" stmt* "}"
//      | declaration
Node *stmt2() {
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

  if (is_typename())
    return declaration();

  Node *node = expr();
  expect(";");
  return node;
}

// declaration = basetype ident ("[" num "]")* ("=" expr) ";"
Node *declaration() {
  Type *ty = basetype();

  char *name = expect_ident();
  ty = read_type_suffix(ty);
  Var *lvar = new_lvar(name, ty);

  if (consume(";"))
    return new_node_null();

  expect("=");
  Node *lhs = new_node_var(lvar);
  Node *rhs = expr();
  Node *node = new_node(ND_ASSIGN, lhs, rhs);
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
      node = new_node_add(node, mul());
    else if (consume("-"))
      node = new_node_sub(node, mul());
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
//       | ("*" | "&") unary
//       | postfix
Node *unary() {
  Token *t;
  if (consume("+"))
    return primary();
  if (consume("-"))
    return new_node(ND_SUB, new_node_num(0), primary());
  if (consume("*"))
    return new_node_unary(ND_DEREF, unary());
  if (consume("&"))
    return new_node_unary(ND_ADDR, unary());
  return postfix();
}

// postfix = primary ("[" expr "]")*
Node *postfix() {
  Node *node = primary();

  while (consume("[")) {
    // x[y] は *(x+y) と同じ意味
    Node *exp = new_node_add(node, expr());
    expect("]");
    node = new_node_unary(ND_DEREF, exp);
  }

  return node;
}

// stmt-expr = "(" "{" stmt stmt* "}" ")"
Node *stmt_expr_tail() {
  Node *node = new_node_stmt_expr();
  node->body = stmt();
  Node *cur = node->body;

  while(!consume("}")){
    cur->next = stmt();
    cur = cur->next;
  }
  expect(")");

  return node;
}

// primary = "(" "{" stmt-expr "}" ")"
//         | "(" expr ")"
//         | "sizeof" unary
//         | ident func-args?
//         | str
//         | num
Node *primary() {
  Token *tok;

  if (consume("(")) {
    if(consume("{"))
      return stmt_expr_tail();

    Node *node = expr();
    expect(")");
    return node;
  }

  if (consume("sizeof")) {
    Node *node = unary();
    add_type(node);
    return new_node_num(node->ty->size);
  }

  if ((tok = consume_ident())) {
    if (consume("(")) {
      Node *node = new_node_fun_call(strndup(tok->str, tok->len));
      return node;
    }

    Var *var = find_var(tok);
    if (!var) {
      error("undefined variable");
    }
    return new_node_var(var);
  }

  tok = token;
  if (tok->kind == TK_STR) {
    token = token->next;

    Type *ty = array_of(char_type, tok->cont_len);
    Var *var = new_gvar(new_label(), ty);
    var->contents = tok->contents;
    var->cont_len = tok->cont_len;
    return new_node_var(var);
  }

  return new_node_num(expect_number());
}
