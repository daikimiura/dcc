//
// Created by 三浦大輝 on 2020/04/07.
//

#include "dcc.h"

Type *char_type = &(Type) {TY_CHAR, 1, 1};
Type *short_type = &(Type) {TY_SHORT, 2, 2};
Type *int_type = &(Type) {TY_INT, 4, 4};
Type *long_type = &(Type) {TY_LONG, 8, 8};

bool is_integer(Type *ty) {
  return ty->kind == TY_INT || ty->kind == TY_SHORT || ty->kind == TY_LONG || ty->kind == TY_CHAR;
}

Type *new_type(TypeKind kind, int size, int align) {
  Type *ty = calloc(1, sizeof(Type));
  ty->kind = kind;
  ty->size = size;
  ty->align = align;
  return ty;
}

Type *pointer_to(Type *ptr_to) {
  Type *ty = new_type(TY_PTR, 8, 8);
  ty->ptr_to = ptr_to;
  return ty;
}

Type *array_of(Type *pointer_to, int len) {
  Type *ty = new_type(TY_ARRAY, pointer_to->size * len, pointer_to->align);
  ty->ptr_to = pointer_to;
  ty->array_len = len;
  return ty;
}

Type *func_type(Type *return_ty) {
  Type *ty = new_type(TY_FUNC, 1, 1);
  ty->return_ty = return_ty;
  return ty;
}

// n以上の整数のうち、alignで割り切れる最小の整数を返す
// alignは2の冪乗
// align_to(33, 8) => 40
int align_to(int n, int align) {
  // ~(align - 1) => align以上のbitを全て立てる
  // (n + align - 1) => 最大でも align - 1を足せばalignの倍数になるはず
  return (n + align - 1) & ~(align - 1);
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
    case ND_VAR: // 変数
      node->ty = node->var->ty;
      return;
    case ND_MEMBER: // 構造体のメンバ
      node->ty = node->member->ty;
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
    case ND_STMT_EXPR: {
      Node *last = node->body;
      while (last->next)
        last = last->next;
      node->ty = last->ty;
      return;
    }
  }
}
