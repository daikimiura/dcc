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
  Function *prog = program();

  // 関数ごとにローカル変数にオフセットを割り当てる
  for (Function *fn = prog; fn; fn = fn->next) {
    int offset = 0;
    for (LVar *var = prog->locals; var; var = var->next) {
      offset += 8;
      var->offset = offset;
    }
    fn->stack_size = offset;
  }

  codegen(prog);
  return 0;
}
