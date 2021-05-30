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
typedef struct Relocation Relocation;
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
  Token* next;
  TokenKind kind;

  Type* type;

  int line;
  char* loc;

  int len;

  int64_t int_val;

  char* str_val;
  int str_val_len;
};

struct Member {
  Member* next;
  Type* type;
  Token* token;
  char* name;
  int offset;
  int alignment;
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
  TY_FUNC,
  TY_ARRAY,
} TypeKind;

struct Type {
  TypeKind kind;

  int size;
  int alignment;

  bool is_unsigned;

  Member* members;

  bool is_flexible;
  Type* base;
  int len;

  Type* params;
  bool is_variadic;
  Type* return_type;

  Type* next;
};

typedef enum {
  ND_BLOCK,
  ND_IF,
  ND_SWITCH,
  ND_CASE,
  ND_FOR,
  ND_RETURN,
  ND_LABEL,
  ND_GOTO,
  ND_EXPR_STMT,
  ND_ASSIGN,
  ND_COMMA,
  ND_COND,
  ND_OR,
  ND_AND,
  ND_BITOR,
  ND_BITXOR,
  ND_BITAND,
  ND_EQ,
  ND_NE,
  ND_LT,
  ND_LE,
  ND_LSHIFT,
  ND_RSHIFT,
  ND_ADD,
  ND_SUB,
  ND_MUL,
  ND_DIV,
  ND_MOD,
  ND_CAST,
  ND_NEG,
  ND_ADDR,
  ND_DEREF,
  ND_NOT,
  ND_BITNOT,
  ND_STMT_EXPR,
  ND_GVAR,
  ND_LVAR,
  ND_MEMBER,
  ND_FUNCCALL,
  ND_NUM,
  ND_NULL,
  ND_MEMZERO,
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

  Node* cases;

  Node* args;
  Node* body;

  char* name;

  int offset;

  int64_t val;

  Node* labels;
  Node* gotos;
  char* label;
  char* label_id;
  char* default_label_id;
  char* break_label_id;
  char* continue_label_id;
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

  Node* params;
  Obj* lvars;
  Obj* va_area;
  Node* body;
  int stack_size;

  int offset;
  int alignment;

  Relocation* relocs;

  char* val;
};

struct Relocation {
  Relocation* next;
  char* label;
  int offset;
  long addend;
};

extern char* input_filename;
extern char* output_filename;

extern Type* ty_void;
extern Type* ty_bool;
extern Type* ty_char;
extern Type* ty_short;
extern Type* ty_int;
extern Type* ty_long;
extern Type* ty_uchar;
extern Type* ty_ushort;
extern Type* ty_uint;
extern Type* ty_ulong;

bool is_numable(Type* type);

void error(char* fmt, ...);
void error_token(Token* token, char* fmt, ...);
void warn_token(Token* token);

bool equal_to_token(Token* token, char* s);
bool consume_token(Token** token, char* s);
void expect_token(Token** token, char* s);
int expect_num(Token** token);

Token* tokenize();
Obj* parse(Token* tokens);
void gen(Obj* codes);
