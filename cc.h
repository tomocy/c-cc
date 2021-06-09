#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <libgen.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct Token Token;
typedef struct Node Node;
typedef struct TopLevelObj TopLevelObj;
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
  bool is_bol;

  int len;

  int64_t int_val;
  double float_val;

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
  TY_FLOAT,
  TY_DOUBLE,
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

  Token* ident;
  char* name;

  Type* return_type;
  Type* params;
  bool is_variadic;

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
  ND_FUNC,
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

  int64_t int_val;
  double float_val;

  bool is_definition;

  Node* labels;
  Node* gotos;
  char* label;
  char* label_id;
  char* default_label_id;
  char* break_label_id;
  char* continue_label_id;
};

struct TopLevelObj {
  TopLevelObj* next;
  Obj* obj;
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

Type* new_int_type();
Type* new_uint_type();
Type* new_long_type();
Type* new_ulong_type();
Type* new_float_type();
Type* new_double_type();

bool is_integer(Type* type);
bool is_float(Type* type);

void error(char* fmt, ...);
void error_token(Token* token, char* fmt, ...);
void warn_token(Token* token);

bool equal_to_token(Token* token, char* s);
bool consume_token(Token** token, char* s);
void expect_token(Token** token, char* s);
int expect_num(Token** token);

Token* tokenize();
Token* preprocess(Token* tokens);
TopLevelObj* parse(Token* tokens);
void gen(TopLevelObj* codes);
