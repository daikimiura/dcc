//
// Created by 三浦大輝 on 2020/02/20.
//

#include "dcc.h"

// n以上の整数のうち、alignで割り切れる最小の整数を返す
// align_to(33, 8) => 40
int align_to(int n, int align) {
  // ~(align - 1) => align以上のbitを全て立てる
  // (n + align - 1) => 最大でも align - 1を足せばalignの倍数になるはず
  return (n + align - 1) & ~(align - 1);
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "引数の個数が正しくありません\n");
    return 1;
  }

  // トークナイズしてパースする
  user_input = argv[1];

  token = tokenize(user_input);
  Program *prog = program();

  // 関数ごとにローカル変数にオフセットを割り当てる
  for (Function *fn = prog->fns; fn; fn = fn->next) {
    int offset = 0;
    for (VarList *vl = fn->locals; vl; vl = vl->next) {
      Var *var = vl->var;
      offset += var->ty->size;
      var->offset = offset;
    }
    // スタックサイズを8の倍数に整える
    fn->stack_size = align_to(offset, 8);
  }

  codegen(prog);
  return 0;
}
