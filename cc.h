#define _POSIX_C_SOURCE 200809L
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
  char* str;
  int len;
  int val;
};

typedef struct Type Type;

typedef struct Member Member;

struct Member {
  Member* next;
  Type* type;
  char* name;
  int offset;
};

typedef enum {
  TY_UNAVAILABLE,
  TY_CHAR,
  TY_INT,
  TY_STRUCT,
  TY_PTR,
  TY_ARRAY,
} TypeKind;

struct Type {
  TypeKind kind;
  int size;
  Type* base;
  int len;
  Member* members;
};

extern Type* ty_char;
extern Type* ty_int;

typedef enum {
  ND_BLOCK,
  ND_IF,
  ND_FOR,
  ND_RETURN,
  ND_ASSIGN,
  ND_COMMA,
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
  ND_MEMBER,
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
  Obj* lvars;
  Node* params;
  Node* body;
  int offset;
  char* data;
  int val;
};

typedef struct ScopedVar ScopedVar;

struct ScopedVar {
  ScopedVar* next;
  Obj* var;
};

typedef struct Scope Scope;

struct Scope {
  Scope* next;
  ScopedVar* vars;
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