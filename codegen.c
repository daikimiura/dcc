//
// Created by 三浦大輝 on 2020/02/20.
//

#include "dcc.h"

static void gen(Node *node);

// アセンブリのラベル番号(連番)
static int labelseq = 1;

// break時にjmpするラベルの番号
static int brkseq;

// continue時にjmpするラベルの番号
static int contseq;

// 実行中の関数の名前
static char *funcname;

// 64bitの値を保持するためのレジスタ
// x86_64のABI(Application Binary Interface)で決まっている
static char *argreg8[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

// 32bitの値を保持するためのレジスタ
static char *argreg4[] = {"edi", "esi", "edx", "ecx", "r8d", "r9d"};

// 16bitの値を保持するためのレジスタ
static char *argreg2[] = {"di", "si", "dx", "cx", "r8w", "r9w"};

// 8bitの値を保持するためのレジスタ
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
      // 複合リテラルの場合
      if (node->init)
        gen(node->init);

      Var *var = node->var;

      if (var->is_local) {
        printf("  mov rax, rbp\n");
        printf("  sub rax, %d\n", node->var->offset);
        printf("  push rax\n");
      } else {
        if (var->is_extern)
          printf("  push [_%s@GOTPCREL + rip]\n", var->name);
        else
          // https://kawasin73.hatenablog.com/entry/2019/01/05/183917
          printf("  push [_%s@GOTPCREL + rip]\n", var->name);
      }

      return;
    }
    case ND_DEREF:
      // deref対象のポインタ型変数の値をそのまま返せばいい
      gen(node->lhs);
      return;
    case ND_MEMBER:
      // 構造体変数のアドレスを取得
      gen_addr(node->lhs);
      printf("  pop rax\n");

//   取得したアドレスからoffset分上のアドレスに欲しいメンバのアドレスがある
//   ========<上位アドレス>========
//   ...
//     --------------------------------------------
//                                                ↑
//     メンバBのスタック領域(offset = 4, size = 8)   |
//                                                構造体変数全体のスタック領域
//     ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ |
//     メンバAのスタック領域(offset = 0, size = 4)   ↓
//     --------------------------------------------
//     ...
//   ========<下位アドレス>========
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
    // RAXが指しているアドレスから8bitを読み込んで、符号拡張してRAXに入れる
    printf("  movsx rax, byte ptr [rax]\n");
  else if (ty->size == 2)
    // RAXが指しているアドレスから16bitを読み込んで、符号拡張してRAXに入れる
    printf("  movsx rax, word ptr [rax]\n");
  else if (ty->size == 4)
    // RAXが指しているアドレスから32bitを読み込んで、符号拡張してRAXに入れる
    printf("  movsxd rax, dword ptr [rax]\n");
  else {
    assert(ty->size == 8);
    printf("  mov rax, [rax]\n");
  }
  printf("  push rax\n");
}

// スタックから値を2つ(1つ目: 右辺値、2つ目: 左辺のアドレス)ポップして、アドレスに値を保存する。
// そして保存した値をプッシュする
void store(Type *ty) {
  printf("  pop rdi\n");
  printf("  pop rax\n");

  if (ty->kind == TY_BOOL) {
    // true: 1, false: 0
    // _Bool型で1以上の整数だった場合はtrueとして扱う
    printf("  cmp rdi, 0\n");
    printf("  setne dil\n");
    // dilの値を符号拡張せずにrdiに保存する
    printf("  movzx rdi, dil\n");
  }

  if (ty->size == 1)
    // DILはRDIの下位8bit
    // https://www.sigbus.info/compilerbook#%E6%95%B4%E6%95%B0%E3%83%AC%E3%82%B8%E3%82%B9%E3%82%BF%E3%81%AE%E4%B8%80%E8%A6%A7
    printf("  mov [rax], dil\n");
  else if (ty->size == 2)
    // EDIはRDIの下位32bit
    printf("  mov [rax], di\n");
  else if (ty->size == 4)
    // EDIはRDIの下位32bit
    printf("  mov [rax], edi\n");
  else {
    assert(ty->size == 8);
    printf("  mov [rax], rdi\n");
  }
  printf("  push rdi\n");
}

// RAXの値を引数のTypeにキャストする
void truncate(Type *ty) {
  printf("  pop rax\n");

  if (ty->kind == TY_BOOL) {
    printf("  cmp rax, 0\n");
    // RAXの値が0以外だったら1、0だったら0
    // ALはRAXの下位8bit
    printf("  setne al\n");
  }

  // 符号拡張する
  if (ty->size == 1)
    printf("  movsx rax, al\n");
  else if (ty->size == 2)
    printf("  movsx rax, ax\n");
  else if (ty->size == 4)
    printf("  movsxd rax, eax\n");

  printf("  push rax\n");
}

// スタックトップの値をpopしてインクリメントしてスタックにpushする
void inc(Type *ty) {
  printf("  pop rax\n");
  printf("  add rax, %d\n", ty->ptr_to ? ty->ptr_to->size : 1);
  printf("  push rax\n");
}

// スタックトップの値をpopしてデクリメントしてスタックにpushする
void dec(Type *ty) {
  printf("  pop rax\n");
  printf("  sub rax, %d\n", ty->ptr_to ? ty->ptr_to->size : 1);
  printf("  push rax\n");
}

void gen_binary(Node *node) {
  printf("  pop rdi\n"); // rhs
  printf("  pop rax\n"); // lhs

  switch (node->kind) {
    case ND_ADD:
    case ND_ADD_EQ:
      printf("  add rax, rdi\n");
      break;
    case ND_PTR_ADD:
    case ND_PTR_ADD_EQ:
      printf("  imul rdi, %d\n", node->ty->ptr_to->size);
      printf("  add rax, rdi\n"); // raxに入ってるのはアドレス
      break;
    case ND_SUB:
    case ND_SUB_EQ:
      printf("  sub rax, rdi\n");
      break;
    case ND_PTR_SUB:
    case ND_PTR_SUB_EQ:
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
    case ND_MUL_EQ:
      printf("  imul rax, rdi\n");
      break;
    case ND_DIV:
    case ND_DIV_EQ:
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
    case ND_BITAND:
    case ND_BITAND_EQ:
      printf("  and rax, rdi\n");
      break;
    case ND_BITOR:
    case ND_BITOR_EQ:
      printf("  or rax, rdi\n");
      break;
    case ND_BITXOR:
    case ND_BITXOR_EQ:
      printf("  xor rax, rdi\n");
      break;
    case ND_SHL:
    case ND_SHL_EQ:
      printf("  mov cl, dil\n");
      printf("  shl rax, cl\n");
      break;
    case ND_SHR:
    case ND_SHR_EQ:
      printf("  mov cl, dil\n");
      // 論理シフトと算術シフトの違い↓
      // http://kccn.konan-u.ac.jp/information/cs/cyber03/cy3_shc.htm
      printf("  sar rax, cl\n");
      break;
    default:
      break;
  }

  printf("  push rax\n");
}

void gen(Node *node) {
  switch (node->kind) {
    case ND_NULL:
      return;
    case ND_NUM:
      if (node->val == (int) node->val)
        printf("  push %ld\n", node->val);
      else {
        // `push` は64bitの整数を直接プッシュできないので
        // 一旦レジスタにコピーしてからプッシュする
        printf("  movabs rax, %ld\n", node->val);
        printf("  push rax\n");
      }
      return;
    case ND_EXPR_STMT:
      gen(node->lhs);
      printf("  pop rax\n");
      return;
    case ND_RETURN:
      if (node->lhs) {
        gen(node->lhs);
        printf("  pop rax\n");
      }
      printf("  jmp .L.return.%s\n", funcname);
      return;
    case ND_VAR:
      // 複合リテラルの場合
      if (node->init)
        gen(node->init);
      gen_addr(node);
      if (node->ty->kind != TY_ARRAY)
        load(node->ty);
      return;
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
      int brk = brkseq;
      int cont = contseq;
      brkseq = contseq = seq;

      printf(".L.continue.%d:\n", seq);
      gen(node->cond);
      printf("  pop rax\n");
      printf("  cmp rax, 0\n");
      printf("  je .L.break.%d\n", seq);
      gen(node->then);
      printf("  jmp .L.continue.%d\n", seq);
      printf(".L.break.%d:\n", seq);

      brkseq = brk;
      contseq = cont;
      return;
    }
    case ND_FOR: {
      int seq = labelseq++;
      int brk = brkseq;
      int cont = contseq;
      brkseq = contseq = seq;

      if (node->init)
        gen(node->init);

      printf(".L.begin.%d:\n", seq);

      if (node->cond) {
        gen(node->cond);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je .L.break.%d\n", seq);
      }

      gen(node->then);
      printf(".L.continue.%d:\n", seq);

      if (node->inc)
        gen(node->inc);

      printf("  jmp .L.begin.%d\n", seq);
      printf(".L.break.%d:\n", seq);

      brkseq = brk;
      contseq = cont;
      return;
    }
    case ND_DO: {
      int seq = labelseq++;
      int brk = brkseq;
      int cont = contseq;
      brkseq = contseq = seq;

      printf(".L.begin.%d:\n", seq);
      gen(node->then);
      printf(".L.continue.%d:\n", seq);
      gen(node->cond);
      printf("  pop rax\n");
      printf("  cmp rax, 0\n");
      printf("  jne .L.begin.%d\n", seq);
      printf(".L.break.%d:\n", seq);

      brkseq = brk;
      contseq = cont;
      return;
    }
    case ND_SWITCH: {
      int seq = labelseq++;
      int brk = brkseq;
      brkseq = seq;

      gen(node->cond);
      printf("  pop rax\n");

      for (Node *n = node->case_next; n; n = n->case_next) {
        n->case_label = labelseq++;
        printf("  cmp rax, %ld\n", n->val);
        printf("  je .L.case.%d\n", n->case_label);
      }

      if (node->default_case) {
        int i = labelseq++;
        node->default_case->case_label = i;
        printf("  jmp .L.case.%d\n", i);
      }

      printf("  jmp .L.break.%d\n", seq);
      // 実際のcase文の処理はここで展開する
      gen(node->then);
      printf(".L.break.%d:\n", seq);

      brkseq = brk;
      return;
    }
    case ND_CASE:
      printf(".L.case.%d:\n", node->case_label);
      gen(node->lhs);
      return;
    case ND_BLOCK:
    case ND_STMT_EXPR:
      for (Node *n = node->body; n; n = n->next)
        gen(n);
      return;
    case ND_FUNCALL: {
      if(!strcmp(node->funcname, "__builtin_va_start")) {
        // https://uclibc.org/docs/psABI-x86_64.pdf
        printf("  pop rax\n");
        printf("  mov edi, dword ptr [rbp-8]\n");
        printf("  mov dword ptr [rax], 0\n");
        printf("  mov dword ptr [rax+4], 0\n");
        printf("  mov qword ptr [rax+8], rdi\n");
        printf("  mov qword ptr [rax+16], 0\n");
        return;
      }

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
      if (node->ty->kind == TY_BOOL)
        // _Boolを返す関数の場合、RAXの下位8bitのみが立っていないといけない
        printf("  movzx rax, al\n");
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
    case ND_CAST:
      gen(node->lhs);
      truncate(node->ty);
      return;
    case ND_ADD_EQ:
    case ND_PTR_ADD_EQ:
    case ND_SUB_EQ:
    case ND_PTR_SUB_EQ:
    case ND_MUL_EQ:
    case ND_DIV_EQ:
    case ND_SHL_EQ:
    case ND_SHR_EQ:
    case ND_BITAND_EQ:
    case ND_BITOR_EQ:
    case ND_BITXOR_EQ:
      gen_addr(node->lhs);
      printf("  push [rsp]\n");
      load(node->lhs->ty);
      gen(node->rhs);
      gen_binary(node);
      store(node->ty);
      return;
    case ND_COMMA:
      gen(node->lhs);
      gen(node->rhs);
      return;
    case ND_PRE_INC:
      gen_addr(node->lhs);
      // storeする時にスタックが
      // -------<上位アドレス>------
      // [アドレス]
      // [storeする値]
      // -------<下位アドレス>-------
      // のようになってないといけないのでrspのアドレス(左辺値のアドレス)をpushする
      // (loadはスタックからpopしてpushするので、ここで左辺値のアドレスをpushしておかないとstoreできない)
      printf("  push [rsp]\n");
      load(node->ty);
      inc(node->ty);
      store(node->ty);
      return;
    case ND_PRE_DEC:
      gen_addr(node->lhs);
      printf("  push [rsp]\n");
      load(node->ty);
      dec(node->ty);
      store(node->ty);
      return;
    case ND_POST_INC:
      gen_addr(node->lhs);
      printf("  push [rsp]\n");
      load(node->ty);
      inc(node->ty);
      // インクリメントした値をstoreするが、スタックのトップにはインクリメントする前の値(インクリメントしてからデクリメントした値)を残す
      store(node->ty);
      dec(node->ty);
      return;
    case ND_POST_DEC:
      gen_addr(node->lhs);
      printf("  push [rsp]\n");
      load(node->ty);
      dec(node->ty);
      // デクリメントした値をstoreするが、スタックのトップにはデクリメントする前の値(デクリメントしてからインクリメントした値)を残す
      store(node->ty);
      inc(node->ty);
      return;
    case ND_NOT:
      gen(node->lhs);
      printf("  pop rax\n");
      // RAXの値が0だったら1、0以外だったら0に更新
      printf("  cmp rax, 0\n");
      printf("  sete al\n");
      printf("  movsx rax, al\n");
      printf("  push rax\n");
      return;
    case ND_BIT_NOT:
      gen(node->lhs);
      printf("  pop rax\n");
      printf("  not rax\n");
      printf("  push rax\n");
      return;
    case ND_LOGOR: {
      int seq = labelseq++;
      gen(node->lhs);
      // lhsの値が0以外ならtrueへジャンプ
      printf("  pop rax\n");
      printf("  cmp rax, 0\n");
      printf("  jne .L.true.%d\n", seq);
      // rhsの値が0以外ならtrueへジャンプ
      gen(node->rhs);
      printf("  pop rax\n");
      printf("  cmp rax, 0\n");
      printf("  jne .L.true.%d\n", seq);
      // lhsとrhsがどちらも0なら0(false)をpush
      printf("  push 0\n");
      printf("  jmp .L.end.%d\n", seq);
      printf(".L.true.%d:\n", seq);
      printf("  push 1\n");
      printf(".L.end.%d:\n", seq);
      return;
    }
    case ND_LOGAND: {
      int seq = labelseq++;
      gen(node->lhs);
      // lhsの値が0ならfalseへジャンプ
      printf("  pop rax\n");
      printf("  cmp rax, 0\n");
      printf("  je .L.false.%d\n", seq);
      // rhsの値が0ならfalseへジャンプ
      gen(node->rhs);
      printf("  pop rax\n");
      printf("  cmp rax, 0\n");
      printf("  je .L.false.%d\n", seq);
      // lhsとrhsがどちらも0でないなら1(true)をpush
      printf("  push 1\n");
      printf("  jmp .L.end.%d\n", seq);
      printf(".L.false.%d:\n", seq);
      printf("  push 0\n");
      printf(".L.end.%d:\n", seq);
      return;
    }
    case ND_BREAK:
      if (brkseq == 0)
        error("break文が無効です");
      printf("  jmp .L.break.%d\n", brkseq);
      return;
    case ND_CONTINUE:
      if (contseq == 0)
        error("continue文が無効です");
      printf("  jmp .L.continue.%d\n", contseq);
      return;
    case ND_GOTO:
      printf("  jmp .L.label.%s.%s\n", funcname, node->label_name);
      return;
    case ND_LABEL:
      printf(".L.label.%s.%s:\n", funcname, node->label_name);
      gen(node->lhs);
      return;
    case ND_TERNARY: {
      int seq = labelseq++;
      gen(node->cond);
      printf("  pop rax\n");
      printf("  cmp rax, 0\n");
      printf("  je .L.else.%d\n", seq);
      gen(node->then);
      printf("  jmp .L.end.%d\n", seq);
      printf(".L.else.%d:\n", seq);
      gen(node->els);
      printf(".L.end.%d:\n", seq);
      return;
    }
    default:;
  }


  gen(node->lhs);
  gen(node->rhs);
  gen_binary(node);
}

// データ(.data)セクションの内容を出力する
// https://qiita.com/MoriokaReimen/items/b320e6cc82c8873a602f
void emit_data(Program *prog) {
  for (VarList *vl = prog->globals; vl; vl = vl->next) {
    if (!vl->var->is_static)
      printf(".global _%s\n", vl->var->name);
  }

  // 初期化されていないグローバル変数はbss領域に格納
  printf(".bss\n");

  for (VarList *vl = prog->globals; vl; vl = vl->next) {
    Var *gvar = vl->var;

    if (gvar->initializer)
      continue;

    printf(".align %d\n", gvar->ty->align);
    printf("_%s:\n", gvar->name);
    // 指定したバイト数(var->ty->size)を0で埋める
    // https://docs.oracle.com/cd/E26502_01/html/E28388/eoiyg.html
    printf("  .zero %d\n", gvar->ty->size);
  }

  // 初期化されているグローバル変数はdata領域に格納
  printf(".data\n");
  for (VarList *vl = prog->globals; vl; vl = vl->next) {
    Var *gvar = vl->var;

    if (!gvar->initializer)
      continue;
    printf(".align %d\n", gvar->ty->align);
    printf("_%s:\n", gvar->name);

    for (Initializer *init = gvar->initializer; init; init = init->next) {
      if (init->label)
        // 他のグローバル変数への参照
        printf("  .quad _%s%+ld\n", init->label, init->addend);
      else if (init->size == 1)
        printf("  .byte %ld\n", init->val);
      else
        printf("  .%dbyte %ld\n", init->size, init->val);
    }
  }
}

void load_arg(Var *var, int idx) {
  int sz = var->ty->size;
  if (sz == 1) {
    printf("  mov[rbp-%d], %s\n", var->offset, argreg1[idx]);
  } else if (sz == 2) {
    printf("  mov[rbp-%d], %s\n", var->offset, argreg2[idx]);
  } else if (sz == 4) {
    printf("  mov[rbp-%d], %s\n", var->offset, argreg4[idx]);
  } else {
    assert(sz == 8);
    printf("  mov[rbp-%d], %s\n", var->offset, argreg8[idx]);
  }
}

void emit_text(Program *prog) {
  printf(".text\n");

  for (Function *fn = prog->fns; fn; fn = fn->next) {
    funcname = fn->name;
    if (!fn->is_static)
      printf(".global _%s\n", funcname);
    printf("_%s:\n", funcname);

    // プロローグ
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    printf("  sub rsp, %d\n", fn->stack_size);

    if(fn->has_varargs) {
      int n= 0;
      for(VarList *vl = fn->params; vl; vl = vl->next)
        n++;

      printf("mov dword ptr [rbp-8], %d\n", n * 8);
      printf("mov [rbp-16], r9\n");
      printf("mov [rbp-24], r8\n");
      printf("mov [rbp-32], rcx\n");
      printf("mov [rbp-40], rdx\n");
      printf("mov [rbp-48], rsi\n");
      printf("mov [rbp-56], rdi\n");
    }

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
