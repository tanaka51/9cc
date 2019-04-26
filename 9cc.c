#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// トークンの型を表す値
enum {
  TK_NUM = 256, // 整数トークン
  TK_EOF,       // 入力の終わりを表すトークン
};

// トークンの型
typedef struct {
  int ty;      // トークンの型
  int val;     // tyがTK_NUMの場合、その数値
  char *input; // トークン文字列（エラーメッセージ用）
} Token;

// トークナイズした結果のトークン列はこの配列に保存する
// 100個以上のトークンは来ないものとする
Token tokens[100];
// 現在処理中のトークンのポジションを表す
int pos = 0;

// ノードの型を表す値
enum {
  ND_NUM = 256, // 整数ノードの型
};

// ノードの型
typedef struct Node {
  int ty;           // 演算子かND_NUM
  struct Node *lhs; // 左辺
  struct Node *rhs; // 右辺
  int val;           // tyがND_NUMの場合のみ使う
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
  int i = 0;

  while (*p) {
    // 空白文字をスキップ
    if (isspace(*p)) {
      p++;
      continue;
    }

    // XXX: 適当に */() を足してる
    if (*p == '+' || *p == '-' || *p == '*' || *p == '/' || *p == '(' || *p == ')') {
      tokens[i].ty = *p;
      tokens[i].input = p;

      i++;
      p++;

      continue;
    }

    if (isdigit(*p)) {
      tokens[i].ty = TK_NUM;
      tokens[i].input = p;
      tokens[i].val = strtol(p, &p, 10);

      i++;

      continue;
    }

    error("トークナイズできません: %s", p);
  }

  tokens[i].ty = TK_EOF;
  tokens[i].input = p;
}

int consume(int ty) {
  if (tokens[pos].ty != ty) {
    return 0;
  }

  pos++;
  return 1;
}

Node *add();

Node *term() {
  if (consume('(')) {
    Node *node = add();
    if (!consume(')')) {
      error("開きカッコに対応するカッコがありません: %s", tokens[pos].input);
    }

    return node;
  }

  if (tokens[pos].ty == TK_NUM) {
    return new_node_num(tokens[pos++].val);
  }

  error("数値でも開きカッコでもないトークンです: %s", tokens[pos].input);

  return NULL;
}

Node *mul() {
  Node *node = term();

  for (;;) {
    if (consume('*')) {
      node = new_node('*', node, term());
    } else if (consume('/')) {
      node = new_node('/', node, term());
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
  } else {
    printf("%c", node->ty);
  }
  printf("\n");
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "引数の数が正しくありません\n");

    return 1;
  }

  tokenize(argv[1]);
  Node *node;
  node = add();
  print_node(node, 0);

  printf(".intel_syntax noprefix\n");
  printf(".global main\n");
  printf("main:\n");

  // 式の最初は数でなければならないので、それをチェックして
  // 最初のmov命令を出力
  if (tokens[0].ty != TK_NUM) {
    error("最初の項が数ではありません");
  }
  printf("  mov rax, %d\n", tokens[0].val);

  // `+ <数>`あるいは`- <数>`というトークンの並びを消費しつつ
  // アセンプリを出力
  int i = 1;
  while (tokens[i].ty != TK_EOF) {
    if (tokens[i].ty == '+') {
      i++;
      if (tokens[i].ty != TK_NUM) {
        error("予期しないトークンです: %s", tokens[i].input);
      }
      printf("  add rax, %d\n", tokens[i].val);
      i++;
      continue;
    }

    if (tokens[i].ty == '-') {
      i++;
      if (tokens[i].ty != TK_NUM) {
        error("予期しないトークンです: %s", tokens[i].input);
      }
      printf("  sub rax, %d\n", tokens[i].val);
      i++;
      continue;
    }

    error("予期しないトークンです: %s", tokens[i].input);
  }

  printf("  ret\n");

  return 0;
}
