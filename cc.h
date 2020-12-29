#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef struct Token Token;
typedef struct Node Node;
typedef struct Obj Obj;
typedef struct Type Type;
typedef struct Member Member;

typedef enum {
  TK_RESERVED,
  TK_NUM,
  TK_STR,
  TK_IDENT,
  TK_EOF,
} TokenKind;

struct Token {
  TokenKind kind;
  Token* next;
  int line;
  char* loc;
  int len;
  int64_t int_val;
  char* str_val;
};

struct Member {
  Member* next;
  Type* type;
  char* name;
  int offset;
};

typedef enum {
  TY_VOID,
  TY_BOOL,
  TY_CHAR,
  TY_SHORT,
  TY_INT,
  TY_LONG,
  TY_STRUCT,
  TY_UNION,
  TY_PTR,
  TY_ARRAY,
} TypeKind;

struct Type {
  TypeKind kind;
  int size;
  int alignment;
  Member* members;
  Type* base;
  int len;
};

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
  ND_NOT,
  ND_BITNOT,
  ND_CAST,
  ND_MUL,
  ND_DIV,
  ND_MOD,
  ND_STMT_EXPR,
  ND_NUM,
  ND_GVAR,
  ND_LVAR,
  ND_MEMBER,
  ND_FUNCCALL,
} NodeKind;

struct Node {
  Node* next;
  NodeKind kind;
  Type* type;
  Token* token;
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
  int64_t val;
};

typedef enum {
  OJ_FUNC,
  OJ_GVAR,
  OJ_LVAR,
  OJ_ENUM,
  OJ_DEF_TYPE,
  OJ_TAG,
} ObjKind;

struct Obj {
  Obj* next;
  ObjKind kind;
  Type* type;
  char* name;
  bool is_static;
  bool is_definition;
  Obj* lvars;
  Node* params;
  Node* body;
  int stack_size;
  int offset;
  char* str_val;
  int int_val;
};

extern char* input_filename;

extern char* output_filename;

void error(char* fmt, ...);

void error_token(Token* token, char* fmt, ...);

void warn_token(Token* token);

bool equal_to_token(Token* token, char* s);

bool consume_token(Token** token, char* s);

void expect_token(Token** token, char* s);

int expect_num(Token** token);

Token* tokenize();

bool is_numable(Type* type);

Obj* parse(Token* tokens);

void gen(Obj* codes);
