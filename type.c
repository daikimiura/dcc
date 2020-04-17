//
// Created by 三浦大輝 on 2020/04/07.
//

#include "dcc.h"

Type *int_type = &(Type) {TY_INT, 8};

bool is_integer(Type *ty) {
  return ty->kind == TY_INT;
}

Type *pointer_to(Type *ptr_to) {
  Type *ty = calloc(1, sizeof(Type));
  ty->kind = TY_PTR;
  ty->size = 8;
  ty->ptr_to = ptr_to;
  return ty;
}

Type *array_of(Type *pointer_to, int len) {
  Type *ty = calloc(1, sizeof(Type));
  ty->kind = TY_ARRAY;
  ty->size = pointer_to->size * len;
  ty->ptr_to = pointer_to;
  ty->array_len = len;
  return ty;
}

// Nodeに型の情報を再帰的に追加する
void add_type(Node *node) {
  if (!node || node->ty)
    return;

  add_type(node->lhs);
  add_type(node->rhs);
  add_type(node->cond);
  add_type(node->then);
  add_type(node->els);
  add_type(node->init);
  add_type(node->inc);

  for (Node *n = node->body; n; n = n->next)
    add_type(n);

  for (Node *n = node->args; n; n = n->next)
    add_type(n);

  switch (node->kind) {
    case ND_ADD: // num + num
    case ND_SUB: // num - num
    case ND_MUL: // *
    case ND_DIV: // /
    case ND_EQ: // ==
    case ND_NE: // !=
    case ND_LT: // <
    case ND_LE: // <=
    case ND_FUNCALL: // 関数呼び出し
    case ND_NUM: // 整数
      node->ty = int_type;
      return;
    case ND_PTR_ADD: // ptr + num, num + ptr
    case ND_PTR_SUB: // ptr - num
    case ND_PTR_DIFF: // ptr - ptr
    case ND_ASSIGN: // =
      node->ty = node->lhs->ty;
      return;
    case ND_LVAR: // 変数
      node->ty = node->lvar->ty;
      return;
    case ND_ADDR: // &
      if (node->lhs->ty->kind == TY_ARRAY)
        node->ty = pointer_to(node->lhs->ty->ptr_to);
      else {
        node->ty = pointer_to(node->lhs->ty);
      }
      return;
    case ND_DEREF: // *
      if (!node->lhs->ty->ptr_to)
        error("invalid pointer dereference");
      node->ty = node->lhs->ty->ptr_to;
      return;
  }
}
