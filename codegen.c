//
// Created by 三浦大輝 on 2020/02/20.
//

#include "dcc.h"

static void gen(Node *node);

// アセンブリのラベル番号(連番)
static int labelseq = 1;

// 実行中の関数の名前
static char *funcname;

// 64bitの引数を保持するためのレジスタ
// x86_64のABI(Application Binary Interface)で決まっている
static char *argreg8[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

// 8bitの引数を保持するためのレジスタ
static char *argreg1[] = {"dil", "sil", "dl", "cl", "r8b", "r9b"};

// 与えられたノードが変数を指しているときに、その変数のアドレスを計算して、それをスタックにプッシュする
// `x=3; y=&x; *y=5;` のような場合、`*y=5;`を評価する時にND_ASSIGNからこの関数が呼ばれ、node->kindがND_DEREFとなる。
// この時yの値はアドレスを表す整数であるとみなしてよいので、yの値を右辺値として評価すればよい。
// yを右辺値として評価した値はload()時にすでにスタックにpushされている。
//
// それ以外の場合にはエラーを返す
void gen_addr(Node *node) {
  switch (node->kind) {
    case ND_VAR: {
      Var *var = node->var;

      if (var->is_local) {
        printf("  mov rax, rbp\n");
        printf("  sub rax, %d\n", node->var->offset);
        printf("  push rax\n");
      } else {
        // http://tepe.tec.fukuoka-u.ac.jp/HP98/studfile/grth/gt08.pdf
        // http://www.tamasoft.co.jp/lasm/help/lasm9igk.htm
        printf("  push [%s@GOTPCREL + rip]\n", var->name);
      }

      return;
    }
    case ND_DEREF:
      gen(node->lhs);
      return;
    case ND_MEMBER:
      // struct型の変数のアドレスを取得
      gen_addr(node->lhs);
      printf("  pop rax\n");
      // 取得したアドレスからoffset分上のアドレスに欲しいメンバのアドレスがある
      printf("  add rax, %d\n", node->member->offset);
      printf("  push rax\n");
      return;
    default:
      error("ローカル変数ではありません");
  }
}

// スタックからポップしたアドレスから値をロードし、スタックにプッシュする
void load(Type *ty) {
  printf("  pop rax\n");
  if (ty->size == 1)
    // RAXが指しているアドレスから1バイトを読み込んで、符号拡張してRAXに入れる
    printf("  movsx rax, byte ptr [rax]\n");
  else
    printf("  mov rax, [rax]\n");
  printf("  push rax\n");
}

// スタックから値を2つ(1つ目: 右辺値、2つ目: 左辺のアドレス)ポップして、アドレスに値を保存する。
// そして保存した値をプッシュする
void store(Type *ty) {
  printf("  pop rdi\n");
  printf("  pop rax\n");
  if (ty->size == 1)
    // DILはRDIの下位8bit
    // https://www.sigbus.info/compilerbook#%E6%95%B4%E6%95%B0%E3%83%AC%E3%82%B8%E3%82%B9%E3%82%BF%E3%81%AE%E4%B8%80%E8%A6%A7
    printf("  mov [rax], dil\n");
  else
    printf("  mov [rax], rdi\n");
  printf("  push rdi\n");
}

void gen(Node *node) {
  switch (node->kind) {
    case ND_NULL:
      return;
    case ND_NUM:
      printf("  push %d\n", node->val);
      return;
    case ND_EXPR_STMT:
      gen(node->lhs);
      printf("  pop rax\n");
      return;
    case ND_RETURN:
      gen(node->lhs);
      printf("  pop rax\n");
      printf("  jmp .L.return.%s\n", funcname);
      return;
    case ND_VAR:
    case ND_MEMBER:
      gen_addr(node);
      if (node->ty->kind != TY_ARRAY)
        load(node->ty);
      return;
    case ND_ASSIGN:
      gen_addr(node->lhs);
      gen(node->rhs);
      store(node->ty);
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
    case ND_STMT_EXPR:
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
        printf("  pop %s\n", argreg8[i]);

      // 関数呼び出しをする前にRSPが16の倍数になっていなければいけない(ABIで決まっている)
      int seq = labelseq++;
      printf("  mov rax, rsp\n");
      // 16の倍数だったら下位4ビットは全て0のはず
      // つまり15との論理和を取ったら0になるはず
      // 16の倍数じゃなかったらRSPに8から引く(RSPは必ず8の倍数か16の倍数になる)
      printf("  and rax, 15\n");
      printf("  jne .L.call.%d\n", seq);
      printf("  mov rax, 0\n");
      printf("  call _%s\n", node->funcname);
      printf("  jmp .L.end.%d\n", seq);
      printf(".L.call.%d:\n", seq);
      printf("  sub rsp, 8\n");
      printf("  mov rax, 0\n");
      printf("  call _%s\n", node->funcname);
      printf("  add rsp, 8\n"); // 引いた分を戻す
      printf(".L.end.%d:\n", seq);
      printf("  push rax\n");
      return;
    }
    case ND_ADDR:
      gen_addr(node->lhs);
      return;
    case ND_DEREF:
      gen(node->lhs);
      if (node->ty->kind != TY_ARRAY)
        load(node->ty);
      return;
    default:;
  }

  gen(node->lhs);
  gen(node->rhs);

  printf("  pop rdi\n"); // rhs
  printf("  pop rax\n"); // lhs

  switch (node->kind) {
    case ND_ADD:
      printf("  add rax, rdi\n");
      break;
    case ND_PTR_ADD:
      printf("  imul rdi, %d\n", node->ty->ptr_to->size);
      printf("  add rax, rdi\n"); // raxに入ってるのはアドレス
      break;
    case ND_SUB:
      printf("  sub rax, rdi\n");
      break;
    case ND_PTR_SUB:
      printf("  imul rdi, %d\n", node->ty->ptr_to->size);
      printf("  sub rax, rdi\n");
      break;
    case ND_PTR_DIFF:
      printf("  sub rax, rdi\n");
      printf("  cqo\n");
      printf("  mov rdi, %d\n", node->ty->ptr_to->size);
      printf(
          "  idiv rdi\n"); // idivは暗黙のうちにRDXとRAXを取って、それを合わせたものを128ビット整数とみなして、それを引数のレジスタの64ビットの値で割り、商をRAXに、余りをRDXにセットする
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

// データ(.data)セクションの内容を出力する
// https://qiita.com/MoriokaReimen/items/b320e6cc82c8873a602f
void emit_data(Program *prog) {
  printf(".data\n");

  for (VarList *vl = prog->globals; vl; vl = vl->next) {
    Var *gvar = vl->var;
    printf("%s:\n", gvar->name);
    if (!gvar->contents) {
      // 指定したバイト数(var->ty->size)を0で埋める
      // https://docs.oracle.com/cd/E26502_01/html/E28388/eoiyg.html
      printf("  .zero %d\n", gvar->ty->size);
      continue;
    } else {
      for (int i = 0; i < gvar->cont_len; i++) {
        printf("  .byte %d\n", gvar->contents[i]);
      }
    }
  }
}

void load_arg(Var *var, int idx) {
  int sz = var->ty->size;
  if (sz == 1) {
    printf("  mov[rbp-%d], %s\n", var->offset, argreg1[idx]);
  } else {
    printf("  mov[rbp-%d], %s\n", var->offset, argreg8[idx]);
  }
}

void emit_text(Program *prog) {
  printf(".text\n");

  for (Function *fn = prog->fns; fn; fn = fn->next) {
    funcname = fn->name;
    printf(".global _%s\n", funcname);
    printf("_%s:\n", funcname);

    // プロローグ
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    printf("  sub rsp, %d\n", fn->stack_size);

    // ABIで指定されたレジスタに格納されている関数の引数の値を
    // ローカル変数のためのスタック上の領域に書き出す
    int i = 0;
    for (VarList *vl = fn->params; vl; vl = vl->next) {
      load_arg(vl->var, i++);
    }

    for (Node *n = fn->node; n; n = n->next)
      gen(n);

    // エピローグ
    printf(".L.return.%s:\n", funcname);
    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    // 最後の式の値がRAXに残っているのでそれが返り値になる
    printf("  ret\n");
  }
}

void codegen(Program *prog) {
  printf(".intel_syntax noprefix\n");
  emit_data(prog);
  emit_text(prog);
}
