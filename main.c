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
  Node *node = program();

  codegen(node);
  return 0;
}
