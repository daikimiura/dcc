//
// Created by 三浦大輝 on 2020/02/20.
//

#include "dcc.h"


// 与えられたノードが変数を指しているときに、その変数のアドレスを計算して、それをスタックにプッシュし
// それ以外の場合にはエラーを返す
void gen_addr(Node *node) {
  if (node->kind != ND_LVAR)
    error("ローカル変数ではありません");

  printf("  mov rax, rbp\n");
  printf("  sub rax, %d\n", node->lvar->offset);
  printf("  push rax\n");
}

// スタックからポップしたアドレスから値をロードし、スタックにプッシュする
void load() {
  printf("  pop rax\n");
  printf("  mov rax, [rax]\n");
  printf("  push rax\n");
}

// スタックから値を2つ(1つ目: 右辺値、2つ目: 左辺のアドレス)ポップして、アドレスに値を保存する。
// そして保存した値をプッシュする
void store() {
  printf("  pop rdi\n");
  printf("  pop rax\n");
  printf("  mov [rax], rdi\n");
  printf("  push rdi\n");
}

void gen(Node *node) {
  switch (node->kind) {
    case ND_NUM:
      printf("  push %d\n", node->val);
      return;
    case ND_LVAR:
      gen_addr(node);
      load();
      return;
    case ND_ASSIGN:
      gen_addr(node->lhs);
      gen(node->rhs);
      store();
      return;
    default:;
  }

  gen(node->lhs);
  gen(node->rhs);

  printf("  pop rdi\n");
  printf("  pop rax\n");

  switch (node->kind) {
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
    case ND_EQ:
      printf("  cmp rax, rdi\n");
      printf("  sete al\n");
      printf("  movzx rax, al\n");
      break;
    case ND_NE:
      printf("  cmp rax, rdi\n");
      printf("  setne al\n");
      printf("  movzx rax, al\n");
      break;
    case ND_LT:
      printf("  cmp rax, rdi\n");
      printf("  setl al\n");
      printf("  movzx rax, al\n");
      break;
    case ND_LE:
      printf("  cmp rax, rdi\n");
      printf("  setle al\n");
      printf("  movzx rax, al\n");
      break;
    default:
      error("パーズエラー");
      break;
  }

  printf("  push rax\n");
}

void codegen(Node *node) {
  // アセンブリの前半部分を出力
  printf(".intel_syntax noprefix\n");
  printf(".global _main\n");
  printf("_main:\n");

  // プロローグ
  printf("  push rbp\n");
  printf("  mov rbp, rsp\n");

  // ローカル変数にオフセットを割り当てる
  int offset = 0;
  for (LVar *var = locals; var; var = var->next) {
    offset += 8;
    var->offset = offset;
  }
  printf("  sub rsp, %d\n", offset);


  // 抽象構文木を下りながらコード生成
  for (Node *n = node; n; n = n->next) {
    gen(n);
    // スタックトップに式(expr)の評価結果が残っているはずなので
    // スタックが溢れないようにポップしておく
    printf("  pop rax\n");
  }

  // エピローグ
  printf("  mov rsp, rbp\n");
  printf("  pop rbp\n");
  // 最後の式の値がRAXに残っているのでそれが返り値になる
  printf("  ret\n");
}
