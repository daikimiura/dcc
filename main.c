//
// Created by 三浦大輝 on 2020/02/20.
//

#include "dcc.h"

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
    fn->stack_size = offset;
  }

  codegen(prog);
  return 0;
}
