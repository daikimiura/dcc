//
// Created by 三浦大輝 on 2020/02/20.
//

#include "dcc.h"

char *filename;
char *user_input;
Token *token;

// tokenize時のエラーを報告するための関数
// printfと同じ引数をとる
void error(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

// tokenize時のエラー内容とエラーが起きた位置を報告するための関数
// 下のようなフォーマットでエラーメッセージを表示する
//
// foo.c:10: x = y + + 5;
//                   ^ 式ではありません
void verror_at(char *loc, char *fmt, va_list ap) {
  // locが含まれる行の開始地点と終了地点を取得
  char *line = loc;
  while (user_input < line && line[-1] != '\n')
    line--;

  char *end = loc;
  while (*end != '\n')
    end++;

  // 見つかった行が全体の何行目なのかを調べる
  int line_num = 1;
  for (char *p = user_input; p < line; p++) {
    if (*p == '\n')
      line_num++;
  }

  int indent = fprintf(stderr, "%s:%d: ", filename, line_num);
  fprintf(stderr, "%.*s\n", (int) (end - line), line);

  int pos = loc - line + indent;
  fprintf(stderr, "%*s", pos, ""); // pos個の空白を出力
  fprintf(stderr, "^ ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

void error_at(char *loc, char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  verror_at(loc, fmt, ap);
  exit(1);
}

void warn_at(char *loc, char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  verror_at(loc, fmt, ap);
}

// 現在のトークンが期待している記号の時には、現在のトークンを返す
// そうでない場合はNULLを返す
Token *peek(char *op) {
  if (token->kind != TK_RESERVED ||
      strlen(op) != token->len ||
      memcmp(token->str, op, token->len))
    return NULL;
  return token;
}

// 現在のトークンが期待している記号の時には、トークンを1つ読み進めて現在のトークンを返す
// それ以外の場合にはNULLを返す
bool consume(char *op) {
  if (!peek(op))
    return NULL;
  Token *t = token;
  token = token->next;
  return t;
}

// 現在のトークンが識別子(TK_IDENT)の時には、トークンを1つ読み進めて現在のトークンを返す
// それ以外の場合にはNULLを返す
Token *consume_ident() {
  if (token->kind != TK_IDENT)
    return NULL;
  Token *t = token;
  token = token->next;
  return t;
}

// 現在のトークンが期待している記号の時には、トークンを1つ読み進めてtrueを返す
// それ以外の場合はエラーを返す
bool expect(char *op) {
  if (token->kind != TK_RESERVED ||
      strlen(op) != token->len ||
      memcmp(token->str, op, token->len))
    error_at(token->next->str, "'%s'ではありません", op);
  token = token->next;
  return true;
}

// 現在のトークンが数値の場合、トークンを一つ読み進めてその数値を返す
// それ以外の場合はエラーを返す
int expect_number() {
  if (token->kind != TK_NUM)
    error_at(token->next->str, "数値ではありません");
  int val = token->val;
  token = token->next;
  return val;
}

char *expect_ident() {
  if (token->kind != TK_IDENT)
    error_at(token->str, "識別子ではありません");
  char *s = strndup(token->str, token->len);
  token = token->next;
  return s;
}

bool at_eof() {
  return token->kind == TK_EOF;
}

// 新しいトークンを作成してcurに繋げる
Token *new_token(TokenKind kind, Token *cur, char *str, int len) {
  Token *tok = calloc(1, sizeof(Token));
  tok->kind = kind;
  tok->str = str;
  tok->len = len;
  cur->next = tok;
  return tok;
}

bool is_alpha(char c) {
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_';
}

bool is_alnum(char c) {
  return is_alpha(c) || ('0' <= c && c <= '9');
}

char get_escape_char(char c) {
  // http://wisdom.sakura.ne.jp/programming/c/Cdata1.html
  switch (c) {
    case 'a':
      return '\a';
    case 'b':
      return '\b';
    case 't':
      return '\t';
    case 'n':
      return '\n';
    case 'v':
      return '\v';
    case 'f':
      return '\f';
    case 'r':
      return '\r';
    case 'e':
      return 27; // http://www3.nit.ac.jp/~tamura/ex2/ascii.html
    case '0':
      return 0;
    default:
      return c;
  }
}

Token *read_string_literal(Token *cur, char *start) {
  char *p = start + 1;
  char buf[1024];
  int len = 0;

  for (;;) {
    if (len == sizeof(buf))
      error_at(start, "文字列リテラルが長すぎます");
    if (*p == '\0')
      error_at(start, "文字列リテラルが閉じられていません");
    if (*p == '"')
      break;

    if (*p == '\\') {
      p++;
      buf[len++] = get_escape_char(*p++);
    } else {
      buf[len++] = *p++;
    }
  }
  p++;

  // 例えば "abc"; のような入力文字列があった時、startは最初の'"'でpは';'になる
  Token *tok = new_token(TK_STR, cur, start, p - start);
  tok->contents = malloc(len + 1);
  memcpy(tok->contents, buf, len);
  tok->contents[len] = '\0';
  tok->cont_len = len + 1;
  return tok;
}

Token *read_int_literal(Token *cur, char *start) {
  char *p = start;
  int base;

  if (!strncasecmp(p, "0x", 2) && is_alnum(p[2])) { // 16進数のintリテラル
    p += 2;
    base = 16;
  } else if (!strncasecmp(p, "0b", 2) && is_alnum((p[2]))) { // 2進数のintリテラル
    p += 2;
    base = 2;
  } else if (*p == '0') { // 8進数のintリテラル
    base = 8;
  } else {
    base = 10;
  }

  long val = strtol(p, &p, base);
  Token *tok = new_token(TK_NUM, cur, start, p - start);
  tok->val = val;

  return tok;
}

Token *read_char_literal(Token *cur, char *start) {
  char *p = start + 1;

  char c;
  if (*p == '\\') {
    p++;
    c = get_escape_char(*p++);
  } else {
    c = *p++;
  }

  if (*p != '\'')
    error_at(start, "文字型が長すぎます");
  p++;

  // 例えば 'a'; のような入力文字列があった時、startは最初の'\''でpは';'になる
  Token *tok = new_token(TK_NUM, cur, start, p - start);
  tok->val = c;
  return tok;
}

// 入力文字列pをトークナイズしてそれを返す
Token *tokenize(char *p) {
  Token head;
  head.next = NULL;
  Token *cur = &head;

  while (*p) {
    // 空白文字をスキップ
    if (isspace(*p)) {
      p++;
      continue;
    }

    // 行コメント
    if (strncmp(p, "//", 2) == 0) {
      p += 2;
      while (*p != '\n')
        p++;
      continue;
    }

    // ブロックコメント
    if (strncmp(p, "/*", 2) == 0) {
      // http://hitorilife.com/strstr.php
      // 文字列から文字列を探して、渡された文字列が見つかったときはその先頭へのポインタ、見つからなかったときにはNULLを返す
      char *q = strstr(p + 2, "*/");
      if (!q)
        error_at(p, "コメントが閉じられていません");
      p = q + 2;
      continue;
    }

    // 文字列リテラル
    if (*p == '"') {
      cur = read_string_literal(cur, p);
      p += cur->len;
      continue;
    }

    if (strncmp(p, "typedef", 7) == 0 && !is_alnum(p[7])) {
      cur = new_token(TK_RESERVED, cur, p, 7);
      p += 7;
      continue;
    }

    if (strncmp(p, "sizeof", 6) == 0 && !is_alnum(p[6])) {
      cur = new_token(TK_RESERVED, cur, p, 6);
      p += 6;
      continue;
    }

    if (strncmp(p, "struct", 6) == 0 && !is_alnum(p[6])) {
      cur = new_token(TK_RESERVED, cur, p, 6);
      p += 6;
      continue;
    }

    if (strncmp(p, "return", 6) == 0 && !is_alnum(p[6])) {
      cur = new_token(TK_RESERVED, cur, p, 6);
      p += 6;
      continue;
    }

    if (strncmp(p, "static", 6) == 0 && !is_alnum(p[6])) {
      cur = new_token(TK_RESERVED, cur, p, 6);
      p += 6;
      continue;
    }

    if (strncmp(p, "if", 2) == 0 && !is_alnum(p[2])) {
      cur = new_token(TK_RESERVED, cur, p, 2);
      p += 2;
      continue;
    }

    if (strncmp(p, "while", 5) == 0 && !is_alnum(p[5])) {
      cur = new_token(TK_RESERVED, cur, p, 5);
      p += 5;
      continue;
    }

    if (strncmp(p, "for", 3) == 0 && !is_alnum(p[3])) {
      cur = new_token(TK_RESERVED, cur, p, 3);
      p += 3;
      continue;
    }

    if (strncmp(p, "else", 4) == 0 && !is_alnum(p[4])) {
      cur = new_token(TK_RESERVED, cur, p, 4);
      p += 4;
      continue;
    }

    if (strncmp(p, "void", 4) == 0 && !is_alnum(p[4])) {
      cur = new_token(TK_RESERVED, cur, p, 4);
      p += 4;
      continue;
    }


    if (strncmp(p, "int", 3) == 0 && !is_alnum(p[3])) {
      cur = new_token(TK_RESERVED, cur, p, 3);
      p += 3;
      continue;
    }

    if (strncmp(p, "short", 5) == 0 && !is_alnum(p[5])) {
      cur = new_token(TK_RESERVED, cur, p, 5);
      p += 5;
      continue;
    }

    if (strncmp(p, "long", 4) == 0 && !is_alnum(p[4])) {
      cur = new_token(TK_RESERVED, cur, p, 4);
      p += 4;
      continue;
    }

    if (strncmp(p, "enum", 4) == 0 && !is_alnum(p[4])) {
      cur = new_token(TK_RESERVED, cur, p, 4);
      p += 4;
      continue;
    }

    if (strncmp(p, "char", 4) == 0 && !is_alnum(p[4])) {
      cur = new_token(TK_RESERVED, cur, p, 4);
      p += 4;
      continue;
    }

    if (strncmp(p, "_Bool", 5) == 0 && !is_alnum(p[5])) {
      cur = new_token(TK_RESERVED, cur, p, 5);
      p += 5;
      continue;
    }

    if (is_alpha(*p)) {
      char *q = p++;
      while (is_alnum(*p))
        p++;
      cur = new_token(TK_IDENT, cur, q, p - q);
      continue;
    }

    if (*p == '\'') {
      cur = read_char_literal(cur, p);
      p += cur->len;
      continue;
    }

    if (strncmp(p, "==", 2) == 0 || strncmp(p, "!=", 2) == 0 ||
        strncmp(p, "<=", 2) == 0 || strncmp(p, ">=", 2) == 0 ||
        strncmp(p, "->", 2) == 0 || strncmp(p, "++", 2) == 0 ||
        strncmp(p, "--", 2) == 0 || strncmp(p, "+=", 2) == 0 ||
        strncmp(p, "-=", 2) == 0 || strncmp(p, "*=", 2) == 0 ||
        strncmp(p, "/=", 2) == 0 || strncmp(p, "&&", 2) == 0 ||
        strncmp(p, "||", 2) == 0) {
      cur = new_token(TK_RESERVED, cur, p, 2);
      p += 2;
      continue;
    }

    if (*p == '+' || *p == '-' || *p == '*' || *p == '/' || *p == '(' ||
        *p == ')' || *p == '<' || *p == '>' || *p == ';' || *p == '=' ||
        *p == '}' || *p == '{' || *p == ',' || *p == '&' || *p == '[' ||
        *p == ']' || *p == '.' || *p == '!' || *p == '~' || *p == '|' ||
        *p == '^') {
      cur = new_token(TK_RESERVED, cur, p++, 1);
      continue;
    }

    if (isdigit(*p)) {
      cur = read_int_literal(cur, p);
      p += cur->len;
      continue;
    }

    error_at(p, "トークナイズできません");
  }

  new_token(TK_EOF, cur, p, 0);
  return head.next;
}
