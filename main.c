//
// Created by 三浦大輝 on 2020/02/20.
//

#include "dcc.h"

char *read_file(char *path) {
  FILE *fp = fopen(path, "r");
  if (!fp)
    error("%s を開けません: %s", path, strerror(errno));

  int filemax = 10 * 1024 * 1024; // 10 MB
  char *buf = malloc(filemax);
  int size = fread(buf, 1, filemax - 2, fp); // "\n\0"の分(2バイト)を引く
  if (!feof(fp))
    error("%s: ファイルが大きすぎます");

  // ファイルが必ず"\n\0"で終わるようにする
  if (size == 0 || buf[size - 1] != '\n')
    buf[size++] = '\n';
  buf[size] = '\0';
  return buf;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "引数の個数が正しくありません\n");
    return 1;
  }

  // トークナイズしてパースする
  filename = argv[1];
  user_input = read_file(argv[1]);

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
