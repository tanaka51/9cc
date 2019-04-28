#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int is_alnum(char c) {
  return ('a' <= c && c <= 'z') ||
         ('A' <= c && c <= 'Z') ||
         ('0' <= c && c <= '9') ||
         (c == '_');
}

typedef struct {
  void **data;
  int capacity;
  int len;
} Vector;

Vector *new_vector() {
  Vector *vec = malloc(sizeof(Vector));

  vec->data = malloc(sizeof(void *) * 16);
  vec->capacity = 16;
  vec->len = 0;

  return vec;
}

void vec_push(Vector *vec, void *elem) {
  if (vec->capacity == vec->len) {
    vec->capacity *= 2;
    vec->data = realloc(vec->data, sizeof(void *) * vec->capacity);
  }

  vec->data[vec->len++] = elem;
}

// トークンの型を表す値
enum {
  TK_NUM = 256, // 整数トークン
  TK_IDENT,     // 識別子
  TK_RETURN,    // return
  TK_EOF,       // 入力の終わりを表すトークン
};

// トークンの型
typedef struct {
  int ty;      // トークンの型
  int val;     // tyがTK_NUMの場合、その数値
  char name;   // tyがTK_IDENTの場合、その名前
  char *input; // トークン文字列（エラーメッセージ用）
} Token;

// トークナイズした結果のトークン列はこの配列に保存する
Vector *tokens;
// 現在処理中のトークンのポジションを表す
int pos = 0;

// ノードの型を表す値
enum {
  ND_NUM = 256, // 整数ノードの型
  ND_IDENT,     // 識別子のノードの型
  ND_RETURN,    // returnノードの型
};

// ノードの型
typedef struct Node {
  int ty;           // 演算子かND_NUM
  struct Node *lhs; // 左辺
  struct Node *rhs; // 右辺
  int val;          // tyがND_NUMの場合のみ使う
  char name;        // tyがND_IDENTの場合のみ使う
} Node;

Node *new_node(int ty, Node *lhs, Node *rhs) {
  Node *node = malloc(sizeof(Node));

  node->ty = ty;
  node->lhs = lhs;
  node->rhs = rhs;

  return node;
}

Node *new_node_num(int val) {
  Node *node = malloc(sizeof(Node));

  node->ty = ND_NUM;
  node->val = val;

  return node;
}

Node *new_node_ident(char name) {
  Node *node = malloc(sizeof(Node));

  node->ty = ND_IDENT;
  node->name = name;

  return node;
}

// エラーを報告するための関数
// printfと同じ引数を取る
void error(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

// pが指している文字列をトークンに分割してtokensに保存する
void tokenize(char *p) {
  while (*p) {
    // 空白文字をスキップ
    if (isspace(*p)) {
      p++;
      continue;
    }

    if (strncmp(p, "return", 6) == 0 && !is_alnum(p[6])) {
      Token *token = malloc(sizeof(Token));
      token->ty = TK_RETURN;
      token->input = p;

      vec_push(tokens, token);

      p += 6;

      continue;
    }

    if (*p == '+' || *p == '-' || *p == '*' || *p == '/' || *p == '(' || *p == ')' || *p == ';' || *p == '=') {
      Token *token = malloc(sizeof(Token));
      token->ty = *p;
      token->input = p;

      vec_push(tokens, token);

      p++;

      continue;
    }

    if ('a' <= *p && *p <= 'z') {
      Token *token = malloc(sizeof(Token));
      token->ty = TK_IDENT;
      token->input = p;
      token->name = *p;

      vec_push(tokens, token);

      p++;

      continue;
    }

    if (isdigit(*p)) {
      Token *token = malloc(sizeof(Token));
      token->ty = TK_NUM;
      token->input = p;
      token->val = strtol(p, &p, 10);

      vec_push(tokens, token);

      continue;
    }

    error("トークナイズできません: %s", p);
  }

  Token *token = malloc(sizeof(Token));
  token->ty = TK_EOF;
  token->input = p;

  vec_push(tokens, token);
}

int consume(int ty) {
  if (((Token *)tokens->data[pos])->ty != ty) {
    return 0;
  }

  pos++;
  return 1;
}

Node *code[100];

Node *add();

Node *term() {
  if (consume('(')) {
    Node *node = add();
    if (!consume(')')) {
      error("開きカッコに対応するカッコがありません: %s", ((Token *)tokens->data[pos])->input);
    }

    return node;
  }

  Token *token = (Token *)tokens->data[pos];
  if (token->ty == TK_NUM) {
    return new_node_num(((Token *)tokens->data[pos++])->val);
  }
  if (token->ty == TK_IDENT) {
    return new_node_ident(((Token *)tokens->data[pos++])->name);
  }

  error("数値でも開きカッコでもないトークンです: %s", token->input);

  return NULL;
}

Node *unary() {
  if (consume('+')) {
    return term();
  }
  if (consume('-')) {
    return new_node('-', new_node_num(0), term());
  }

  return term();
}

Node *mul() {
  Node *node = unary();

  for (;;) {
    if (consume('*')) {
      node = new_node('*', node, unary());
    } else if (consume('/')) {
      node = new_node('/', node, unary());
    } else {
      return node;
    }
  }
}

Node *add() {
  Node *node = mul();

  for (;;) {
    if (consume('+')) {
      node = new_node('+', node, mul());
    } else if (consume('-')) {
      node = new_node('-', node, mul());
    } else {
      return node;
    }
  }
}

Node *assign() {
  Node *node = add();

  while (consume('=')) {
    node = new_node('=', node, assign());
  }

  return node;
}

Node *stmt() {
  Node *node;

  if (consume(TK_RETURN)) {
    node = malloc(sizeof(Node));
    node->ty = ND_RETURN;
    node->lhs = assign();
  } else {
    node = assign();
  }

  if (!consume(';')) {
    error("';'ではないトークンです: %s\n", ((Token *)tokens->data[pos])->input);
  }

  return node;
}

void program() {
  int i = 0;

  while (((Token *)tokens->data[pos])->ty != TK_EOF) {
    code[i++] = stmt();
  }

  code[i] = NULL;
}

// シンタックスツリーのデバッグプリント用関数
void print_node(Node *node, int level) {
  if (node->lhs)
    print_node(node->lhs, level + 1);
  if (node->rhs)
    print_node(node->rhs, level + 1);

  printf("# ");
  for(int i = 0; i < level*4; i++) {
    printf(" ");
  }
  if (node->ty == ND_NUM) {
    printf("%d", node->val);
  } else if (node->ty == ND_IDENT) {
    printf("%c", node->name);
  } else {
    printf("%c", node->ty);
  }
  printf("\n");
}

void gen_lval(Node *node) {
  if (node->ty != ND_IDENT) {
    error("代入の左辺値が変数ではありません");
  }

  int offset = ('z' - node->name + 1) * 8;
  printf("  mov rax, rbp\n");
  printf("  sub rax, %d\n", offset);
  printf("  push rax\n");
}

void gen(Node *node) {
  if (node->ty == ND_RETURN) {
    gen(node->lhs);
    // スタックの最後の値を rax にセットする
    printf("  pop rax\n");
    // あとはエピローグと同じ
    // (スタック上にセットした変数用のrbpをリセットする処理)
    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    printf("  ret\n");

    return;
  }

  if (node->ty == ND_NUM) {
    printf("  push %d\n", node->val);

    return;
  }

  if (node->ty == ND_IDENT) {
    gen_lval(node);
    printf("  pop rax\n");
    printf("  mov rax, [rax]\n");
    printf("  push rax\n");

    return;
  }

  if (node->ty == '=') {
    gen_lval(node->lhs);
    gen(node->rhs);

    printf("  pop rdi\n");
    printf("  pop rax\n");
    printf("  mov [rax], rdi\n");
    printf("  push rdi\n");

    return;
  }

  gen(node->lhs);
  gen(node->rhs);

  printf("  pop rdi\n");
  printf("  pop rax\n");

  switch (node->ty) {
    case '+':
      printf("  add rax, rdi\n");
      break;
    case '-':
      printf("  sub rax, rdi\n");
      break;
    case '*':
      printf(" mul rdi\n");
      break;
    case '/':
      printf("  mov rdx, 0\n");
      printf("  div rdi\n");
      break;
  }

  printf("  push rax\n");
}

void expect(int line, int expected, int actual) {
  if (expected == actual) {
    return;
  }

  fprintf(stderr, "%d: %d expected, but got %d\n", line, expected, actual);

  exit(1);
}

void runtest() {
  Vector *vec = new_vector();

  expect(__LINE__, 0, vec->len);

  for (int i = 0; i < 100; i++) {
    Token *token = malloc(sizeof(Token));
    token->ty = TK_NUM;
    token->val = i;

    vec_push(vec, token);
  }

  expect(__LINE__, 100, vec->len);
  expect(__LINE__, 0, ((Token *)vec->data[0])->val);
  expect(__LINE__, 50, ((Token *)vec->data[50])->val);
  expect(__LINE__, 99, ((Token *)vec->data[99])->val);

  printf("OK\n");
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "引数の数が正しくありません\n");

    return 1;
  }

  if (strcmp(argv[1], "-test") == 0) {
    runtest();

    return 0;
  }

  tokens = new_vector();
  tokenize(argv[1]);
  program();

  // debug
  for (int i = 0; code[i]; i++) {
    print_node(code[i], 0);
  }

  // アセンブリの前半部分を出力
  printf(".intel_syntax noprefix\n");
  printf(".global main\n");
  printf("main:\n");

  // プロローグ
  // 変数26個分の領域を確保する
  printf("  push rbp\n");
  printf("  mov rbp, rsp\n");
  printf("  sub rsp, 208\n");

  // 先頭の式から順にコードを生成
  for (int i = 0; code[i]; i++) {
    gen(code[i]);

    // 式の評価結果としてスタックに一つの値が残っている
    // はずなので、スタックが溢れないようにポップしておく
    printf("  pop rax\n");
  }

  // エピローグ
  // 最後の式の結果がRAXに残っているのでそれが返り値になる
  printf("  mov rsp, rbp\n");
  printf("  pop rbp\n");
  printf("  ret\n");

  return 0;
}
