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
  Node* next;
  Node* body;
  char* name;
  Node* args;
  int len;
  int val;
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

extern Node* codes[100];

extern Var* local_vars;

void error(char* fmt, ...);

void error_at(char* loc, char* fmt, ...);

bool equal(Token* tok, char* op);

bool consume(char* op);

void expect(char* op);

int expect_number();

bool at_eof();

Token* tokenize();

void program();

void gen_program();