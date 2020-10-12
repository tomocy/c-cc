#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef enum {
  TK_RESERVED,
  TK_NUM,
  TK_EOF,
} TokenKind;

typedef struct Token Token;

struct Token {
  TokenKind kind;
  Token* next;
  int val;
  char* str;
  int len;
};

typedef enum {
  ND_EQ,
  ND_NE,
  ND_LT,
  ND_LE,
  ND_ADD,
  ND_SUB,
  ND_MUL,
  ND_DIV,
  ND_NUM,
} NodeKind;

typedef struct Node Node;

struct Node {
  NodeKind kind;
  Node* lhs;
  Node* rhs;
  int val;
};

extern char* user_input;

extern Token* token;

void error(char* fmt, ...);

void error_at(char* loc, char* fmt, ...);

bool consume(char* op);

void expect(char* op);

int expect_number();

bool at_eof();

Token* new_token(TokenKind kind, Token* cur, char* str, int len);

bool startswith(char* p, char* q);

Token* tokenize();

Node* new_node(NodeKind kind, Node* lhs, Node* rhs);

Node* new_node_num(int val);

Node* expr();

Node* primary();

Node* unary();

Node* mul();

Node* add();

Node* relational();

Node* equality();

Node* expr();

void gen(Node* node);