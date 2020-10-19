#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef enum {
  TK_RESERVED,
  TK_NUM,
  TK_STR,
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
  TY_INT,
  TY_CHAR,
  TY_PTR,
  TY_ARRAY,
} TypeKind;

typedef struct Type Type;

struct Type {
  TypeKind kind;
  int size;
  Type* base;
  int len;
};

typedef enum {
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
  ND_ADDR,
  ND_DEREF,
  ND_MUL,
  ND_DIV,
  ND_STMT_EXPR,
  ND_NUM,
  ND_GVAR,
  ND_LVAR,
  ND_FUNCCALL,
} NodeKind;

typedef struct Obj Obj;

typedef struct Node Node;

struct Node {
  Node* next;
  NodeKind kind;
  Type* type;
  Node* lhs;
  Node* rhs;
  Node* init;
  Node* cond;
  Node* inc;
  Node* then;
  Node* els;
  Node* body;
  Node* args;
  char* name;
  int len;
  int offset;
  int val;
};

typedef enum {
  OJ_FUNC,
  OJ_GVAR,
  OJ_LVAR,
} ObjKind;

struct Obj {
  Obj* next;
  ObjKind kind;
  Type* type;
  char* name;
  int len;
  Obj* lvars;
  Node* params;
  Node* body;
  int offset;
  char* data;
};

extern char* filename;

extern Token* token;

extern Obj* codes;

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