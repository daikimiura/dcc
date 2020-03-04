//
// Created by 三浦大輝 on 2020/02/20.
//

#include "dcc.h"

// アセンブリのラベル番号(連番)
static int labelseq = 1;

// 関数の引数を保持するためのレジスタ
// x86_64のABI(Application Binary Interface)で決まっている
static char *argreg[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

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
    case ND_RETURN:
      gen(node->lhs);
      printf("  pop rax\n");
      printf("  jmp .L.return\n");
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
    case ND_IF: {
      int seq = labelseq++;
      if (node->els) {
        gen(node->cond);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je .L.else.%d\n", seq);
        gen(node->then);
        printf("  jmp .L.end.%d\n", seq);
        printf(".L.else.%d:\n", seq);
        gen(node->els);
        printf(".L.end.%d:\n", seq);
      } else {
        gen(node->cond);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je .L.end.%d\n", seq);
        gen(node->then);
        printf(".L.end.%d:\n", seq);
      }
      return;
    }
    case ND_WHILE: {
      int seq = labelseq++;
      printf(".L.begin.%d:\n", seq);
      gen(node->cond);
      printf("  pop rax\n");
      printf("  cmp rax, 0\n");
      printf("  je .L.end.%d\n", seq);
      gen(node->then);
      printf("  jmp .L.begin.%d\n", seq);
      printf(".L.end.%d:\n", seq);
      return;
    }
    case ND_FOR: {
      int seq = labelseq++;
      if (node->init)
        gen(node->init);

      printf(".L.begin.%d:\n", seq);

      if (node->cond)
        gen(node->cond);

      printf("  pop rax\n");
      printf("  cmp rax, 0\n");
      printf("  je .L.end.%d\n", seq);

      gen(node->then);

      if (node->inc)
        gen(node->inc);

      printf("  jmp .L.begin.%d\n", seq);
      printf(".L.end.%d:\n", seq);
      return;
    }
    case ND_BLOCK:
      for (Node *n = node->body; n; n = n->next)
        gen(n);
      return;
    case ND_FUNCALL: {
      int nargs = 0; // 引数の個数
      for (Node *arg = node->args; arg; arg = arg->next) {
        gen(arg);
        nargs++;
      }

      for (int i = nargs - 1; i >= 0; i--)
        printf("  pop %s\n", argreg[i]);

      printf("  call _%s\n", node->funcname);
      printf("  push rax\n");
      return;
    }
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
  for (Node *n = node; n; n = n->next)
    gen(n);

  // エピローグ
  printf(".L.return:\n");
  printf("  mov rsp, rbp\n");
  printf("  pop rbp\n");
  // 最後の式の値がRAXに残っているのでそれが返り値になる
  printf("  ret\n");
}
