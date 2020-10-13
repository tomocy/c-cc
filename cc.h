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
  TK_IDENT,
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
  ND_FUNC,
  ND_BLOCK,
  ND_IF,
  ND_FOR,
  ND_RETURN,
  ND_ASSIGN,
  ND_EQ,
  ND_NE,
  ND_LT,
  ND_LE,
  ND_ADD,
  ND_SUB,
  ND_MUL,
  ND_DIV,
  ND_NUM,
  ND_VAR,
  ND_FUNCCALL,
} NodeKind;

typedef struct Node Node;

struct Node {
  NodeKind kind;
  Node* lhs;
  Node* rhs;
  Node* init;
  Node* cond;
  Node* inc;
  Node* then;
  Node* els;
  Node* body;
  Node* args;
  Node* params;
  Node* next;
  int val;
  char* name;
  int len;
  int offset;
};

typedef struct Var Var;

struct Var {
  Var* next;
  char* name;
  int len;
  int offset;
};

extern char* user_input;

extern Token* token;

extern Node* codes;

extern Var* local_vars;

void error(char* fmt, ...);

void error_at(char* loc, char* fmt, ...);

bool equal(Token* tok, char* op);

bool consume(char* op);

void expect(char* op);

int expect_num();

bool at_eof();

void tokenize();

void program();

void gen_program();