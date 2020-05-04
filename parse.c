//
// Created by 三浦大輝 on 2020/02/20.
//

#include "dcc.h"

static VarList *locals;
static VarList *globals;
static VarScope *var_scope;
static TagScope *tag_scope;

// 非終端記号を表す関数のプロトタイプ宣言
Program *program();

Function *function();

Node *declaration();

Type *struct_decl();

Member *struct_member();

Type *basetype();

Type *declarator(Type *ty, char **name);

Type *type_suffix(Type *ty);

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

// ブロックスコープの開始
Scope *enter_scope() {
  Scope *sc = calloc(1, sizeof(Scope));
  sc->var_scope = var_scope;
  sc->tag_scope = tag_scope;
  return sc;
}

// ブロックスコープの終了
void leave_scope(Scope *sc) {
  var_scope = sc->var_scope;
  tag_scope = sc->tag_scope;
}

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

// 変数を名前で検索する
// 見つからなかった場合はNULLを返す
VarScope *find_var(Token *tok) {
  for (VarScope *sc = var_scope; sc; sc = sc->next) {
    if (strlen(sc->name) == tok->len && !strncmp(tok->str, sc->name, tok->len)) {
      return sc;
    }
  }
  return NULL;
}

// typedefを検索する
Type *find_typedef(Token *tok) {
  if (tok->kind == TK_IDENT) {
    VarScope *sc = find_var(tok);
    if (sc)
      return sc->type_def;
  }

  return NULL;
}

// 構造体タグを名前で検索する
// 見つからなかった場合はNULLを返す
TagScope *find_tag(Token *tok) {
  for (TagScope *sc = tag_scope; sc; sc = sc->next)
    if (strlen(sc->name) == tok->len == !strncmp(tok->str, sc->name, tok->len))
      return sc;
  return NULL;
}

// var_scopeの先頭に変数/typedefを追加
VarScope *push_var_scope(char *name) {
  VarScope *sc = calloc(1, sizeof(VarScope));
  sc->name = name;
  sc->next = var_scope;
  var_scope = sc;
  return sc;
}

// 新しいローカル変数 or グローバル変数 を追加する
Var *new_var(char *name, Type *ty, bool is_local) {
  Var *var = calloc(1, sizeof(Var));
  var->name = name;
  var->ty = ty;
  var->is_local = is_local;

  return var;
}

// 新しいローカル変数を連結リスト(locals)の先頭に追加する
Var *new_lvar(char *name, Type *ty) {
  Var *lvar = new_var(name, ty, true);
  push_var_scope(name)->var = lvar;

  VarList *vl = calloc(1, sizeof(VarList));
  vl->var = lvar;
  vl->next = locals;
  locals = vl;
  return lvar;
}

// 新しいグローバル変数 or 関数の名前と戻り値の型 をスコープに追加する
// もしemitがtrueならグローバル変数を管理する連結リスト(globals)の先頭にも追加する
Var *new_gvar(char *name, Type *ty, bool emit) {
  Var *gvar = new_var(name, ty, false);
  push_var_scope(name)->var = gvar;

  if (emit) {
    VarList *vl = calloc(1, sizeof(VarList));
    vl->var = gvar;
    vl->next = globals;
    globals = vl;
  }
  return gvar;
}

//  新しい構造体タグを連結リスト(tag_scope)の先頭に追加する
void push_tag_scope(Token *tag, Type *ty) {
  TagScope *sc = calloc(1, sizeof(TagScope));
  sc->next = tag_scope;
  sc->name = strndup(tag->str, tag->len);
  sc->ty = ty;
  tag_scope = sc;
}

// 配列宣言時の型のsuffix(配列の要素数)を読み取る
// type-suffix = ("[" num "]" type-suffix)?
Type *type_suffix(Type *ty) {
  if (!consume("["))
    return ty;
  int size = expect_number();
  expect("]");
  ty = type_suffix(ty);
  return array_of(ty, size);
}

VarList *read_func_param() {
  Type *ty = basetype();
  char *name = NULL;
  ty = declarator(ty, &name);
  ty = type_suffix(ty);
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
  return peek("char") || peek("int") || peek("short") || peek("long") ||
         peek("struct") || peek("void") || find_typedef(token);
}

// program() が function() かどうか判定する
bool is_function() {
  Token *tok = token;
  // tokenを先読みして判定
  // basetype declarator ( ... ならfunc
  Type *ty = basetype();
  char *name = NULL;
  declarator(ty, &name);
  bool is_func = name && consume("(");
  token = tok;
  return is_func;
};

Node *read_expr_stmt() {
  return new_node_unary(ND_EXPR_STMT, expr());
}

// program = (global-var | function)*
Program *program() {
  locals = NULL;
  globals = NULL;

  Function head = {};
  Function *cur = &head;

  while (!at_eof()) {
    if (is_function()) {
      Function *fn = function();
      if (!fn) // 関数のプロトタイプ宣言
        continue;
      cur->next = fn;
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

// global-var = basetype declarator ("[" num "]")* ";"
void global_var() {
  Type *ty = basetype();
  char *name = NULL;
  ty = declarator(ty, &name);
  ty = type_suffix(ty);
  expect(";");
  new_gvar(name, ty, true);
}

// function = basetype declarator "(" params? ")" ("{" stmt* "}" | ";")
// params = param ("," param)*
// param = basetype declarator
Function *function() {
  locals = NULL;

  Type *ty = basetype();
  char *name = NULL;
  ty = declarator(ty, &name);

  // 関数の名前と戻り値の型をスコープに追加する
  new_gvar(name, func_type(ty), false);

  Function *fn = calloc(1, sizeof(Function));
  fn->name = name;

  expect("(");

  Scope *sc = enter_scope();
  fn->params = read_func_params();

  if (consume(";")) {
    leave_scope(sc);
    return NULL;
  }

  expect("{");

  // stmtを連結リストで管理
  Node head = {};
  Node *cur = &head;

  while (!consume("}")) {
    cur->next = stmt();
    cur = cur->next;
  }
  // スコープを戻す
  leave_scope(sc);

  fn->node = head.next;
  fn->locals = locals;
  return fn;
}

// struct-decl = "struct" ident
//             | "struct" ident? "{" struct-member "}"
Type *struct_decl() {
  Token *tag = consume_ident(); // 構造体タグ
  if (tag && !peek("{")) { // 構造体タグを用いた宣言のとき
    TagScope *sc = find_tag(tag);
    if (!sc)
      error_at(tag->str, "構造体タグが定義されていません");
    return sc->ty;
  }

  expect("{");

  // メンバを連結リストで管理
  Member head = {};
  Member *cur = &head;

  while (!consume("}")) {
    cur->next = struct_member();
    cur = cur->next;
  }

  Type *ty = calloc(1, sizeof(Type));
  ty->kind = TY_STRUCT;
  ty->members = head.next;

  int offset = 0;
  for (Member *mem = ty->members; mem; mem = mem->next) {
    offset = align_to(offset, mem->ty->align);
    mem->offset = offset;
    offset += mem->ty->size;

    // 複合データ型のアラインメントは， それに含まれる要素のアラインメントのうち最大のものになる
    // http://www5d.biglobe.ne.jp/~noocyte/Programming/Alignment.html
    if (ty->align < mem->ty->align)
      ty->align = mem->ty->align;
  }
  ty->size = align_to(offset, ty->align);

  if (tag)
    push_tag_scope(tag, ty);

  return ty;
}

// struct-member = basetype declarator ";"
Member *struct_member() {
  Type *ty = basetype();
  char *name = NULL;
  ty = declarator(ty, &name);
  expect(";");

  Member *mem = calloc(1, sizeof(Member));
  mem->name = name;
  mem->ty = ty;
  return mem;
}

// basetype = builtin-typename | struct-decl | typdef-name
// builtin-typename = "int" | "short" | "long" | "char" | "void"
Type *basetype() {
  Type *ty;
  if (consume("char"))
    ty = char_type;
  else if (consume("int"))
    ty = int_type;
  else if (consume("short"))
    ty = short_type;
  else if (consume("long"))
    ty = long_type;
  else if (consume("struct"))
    ty = struct_decl();
  else if (consume("void"))
    ty = void_type;
  else
    ty = find_var(consume_ident())->type_def;

  return ty;
}

// declarator = "*"* ("(" declarator ")" | ident) type-suffix
// ex.) int *x[3];
//      int (*x)[3];
// http://enakai00.hatenablog.com/entry/20110808/1312783316
Type *declarator(Type *ty, char **name) {
  while (consume("*"))
    ty = pointer_to(ty);

  if (consume("(")) {
    // 例えば、int (*x)[3] をパースするとき
    // *new_ty は placeholderへのポインタ型になり、*nameは "x" になる
    // memcpy で placeholder をint型に更新している
    Type *placeholder = calloc(1, sizeof(Type));
    Type *new_ty = declarator(placeholder, name);
    expect(")");
    memcpy(placeholder, type_suffix(ty), sizeof(Type));
    return new_ty;
  }

  *name = expect_ident();
  return type_suffix(ty);
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
//      | "typedef" basetype declarator;
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
      node->init = read_expr_stmt();
      expect(";");
    }

    if (!consume(";")) {
      node->cond = expr();
      expect(";");
    }

    if (!consume(")")) {
      node->inc = read_expr_stmt();
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

    Scope *sc = enter_scope();
    while (!consume("}")) {
      cur->next = stmt();
      cur = cur->next;
    }
    leave_scope(sc);

    node->body = head.next;
    return node;
  }

  if (consume("typedef")) {
    Type *ty = basetype();
    char *name = NULL;
    ty = declarator(ty, &name);
    ty = type_suffix(ty);
    expect(";");
    push_var_scope(name)->type_def = ty;
    return new_node_null();
  }

  if (is_typename())
    return declaration();

  Node *node = read_expr_stmt();
  expect(";");
  return node;
}

// declaration = basetype declarator ("=" expr) ";"
//             | basetype declarator ";"  // 構造体タグを用いた構造体の宣言
Node *declaration() {
  Type *ty = basetype();
  if (consume(";"))
    return new_node_null();

  char *name = NULL;
  ty = declarator(ty, &name);
  ty = type_suffix(ty);
  Var *lvar = new_lvar(name, ty);

  if (consume(";"))
    return new_node_null();

  expect("=");
  Node *lhs = new_node_var(lvar);
  Node *rhs = expr();
  Node *node = new_node(ND_ASSIGN, lhs, rhs);
  expect(";");
  return new_node_unary(ND_EXPR_STMT, node);
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

Member *find_member(Type *ty, char *name) {
  for (Member *mem = ty->members; mem; mem = mem->next)
    if (!(strcmp(mem->name, name)))
      return mem;

  return NULL;
}

Node *struct_ref(Node *lhs) {
  add_type(lhs);
  if (lhs->ty->kind != TY_STRUCT)
    error_at(token->str, "構造体ではありません");

  Token *tok = token;
  Member *mem = find_member(lhs->ty, expect_ident());
  if (!mem)
    error_at(tok->str, "このメンバは定義されていません");

  Node *node = new_node_unary(ND_MEMBER, lhs);
  node->member = mem;
  return node;
}

// postfix = primary ("[" expr "]" | "." ident | "->" ident)*
Node *postfix() {
  Node *node = primary();

  for (;;) {
    if (consume("[")) {
      // x[y] は *(x+y) と同じ意味
      Node *exp = new_node_add(node, expr());
      expect("]");
      node = new_node_unary(ND_DEREF, exp);
      continue;
    }

    if (consume(".")) {
      node = struct_ref(node);
      continue;
    }

    if (consume("->")) {
      // x->y は (*x).y の糖衣構文
      node = new_node_unary(ND_DEREF, node);
      node = struct_ref(node);
      continue;
    }

    return node;
  }
}

// stmt-expr = "(" "{" stmt stmt* "}" ")"
Node *stmt_expr_tail() {
  Node *node = new_node_stmt_expr();
  node->body = stmt();
  Node *cur = node->body;

  Scope *sc = enter_scope();
  while (!consume("}")) {
    cur->next = stmt();
    cur = cur->next;
  }
  leave_scope(sc);
  expect(")");

  if (cur->kind != ND_EXPR_STMT)
    error("voidを返すstatement expressionはサポートされていません");
  memcpy(cur, cur->lhs, sizeof(Node));

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
    if (consume("{"))
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
      add_type(node);

      VarScope *sc = find_var(tok);
      if (sc) {
        if (!sc->var || sc->var->ty->kind != TY_FUNC)
          error_at(tok->str, "関数ではありません");
        node->ty = sc->var->ty->return_ty;
      } else {
        warn_at(tok->str, "関数の暗黙的な宣言が使われました");
        node->ty = int_type; // とりあえずintにしておく
      }
      return node;
    }

    VarScope *sc = find_var(tok);
    if (sc && sc->var)
      return new_node_var(sc->var);
    error_at(tok->str, "変数が定義されていません");
  }

  tok = token;
  if (tok->kind == TK_STR) {
    token = token->next;

    Type *ty = array_of(char_type, tok->cont_len);
    Var *var = new_gvar(new_label(), ty, true);
    var->contents = tok->contents;
    var->cont_len = tok->cont_len;
    return new_node_var(var);
  }

  return new_node_num(expect_number());
}
