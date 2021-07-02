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
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX(x, y) ((x) > (y) ? x : y)
#define MIN(x, y) ((x) < (y) ? x : y)

typedef struct Str Str;
typedef struct File File;
typedef struct Token Token;
typedef struct Node Node;
typedef struct TopLevelObj TopLevelObj;
typedef struct Obj Obj;
typedef struct Relocation Relocation;
typedef struct Type Type;
typedef struct Member Member;

struct Str {
  Str* next;
  char* data;
};

struct File {
  File* next;
  int index;
  char* name;
  char* contents;
};

typedef enum {
  TK_RESERVED,
  TK_PP_NUM,
  TK_NUM,
  TK_STR,
  TK_IDENT,
  TK_EOF,
} TokenKind;

struct Token {
  Token* next;
  TokenKind kind;

  Type* type;

  File* file;
  int line;
  char* loc;
  int len;
  bool is_bol;
  bool has_leading_space;

  Token* original;
  Str* hideset;

  int64_t int_val;
  double float_val;

  char* str_val;
};

struct Member {
  Member* next;
  Type* type;
  Token* token;
  char* name;
  int offset;
  int alignment;

  bool is_bitfield;
  int bit_width;
  int bit_offset;
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

  // The ident and name are used to parse declarations,
  // and those values are not guaranteed to have valid values
  // after it, so use Node.name or Obj.name instead
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
  ND_LOGOR,
  ND_LOGAND,
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

  Node* return_val;

  Node* args;
  bool is_passed_by_stack;

  Node* body;

  char* name;

  Obj* obj;
  Member* mem;

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

  Node* ptr_to_return_val;
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

// main.c
extern Str* include_paths;

// preprocess.c
Token* preprocess(Token* tokens);
void define_builtin_macros();
void define_builtin_macro(char* name, char* raw_body);
void undefine_macro(char* name);

// tokenize.c
void error_token(Token* token, char* fmt, ...);
void warn_token(Token* token, char* fmt, ...);
bool equal_to_token(Token* token, char* s);
bool equal_to_tokens(Token* token, ...);
bool equal_to_ident_token(Token* token, char* s);
bool consume_token(Token** token, char* s);
Token* expect_token(Token** token, char* s);
Token* expect_tokens(Token** tokens, ...);
Token* expect_ident_token(Token** tokens);
Token* tokenize(char* input_filename);
Token* tokenize_in(File* file);
Token* new_token_in(TokenKind kind, File* file, char* loc, int len);
Token* new_eof_token_in(File* file);
Token* copy_token(Token* src);
Token* copy_tokens(Token* src);
bool can_be_keyword(char* c, int len);
void print_tokens(char* output_filename, Token* tokens);

// parse.c
TopLevelObj* parse(Token* tokens);
int align_up(int n, int align);
int64_t const_expr(Token** tokens);

// gen.c
void gen(char* output_filename, TopLevelObj* codes);

// Utils

// error.c
void error(char* fmt, ...);
void vprint_at(File* file, char* loc, char* fmt, va_list args);
void error_at(File* file, char* loc, char* fmt, ...);
void warn_at(File* file, char* loc, char* fmt, ...);

// file.c
extern File* files;
File* new_file(int index, char* name, char* contents);
File* copy_file_with_contents(File* src, char* contents);
char* replace_file_ext(char* name, char* ext);
char* new_tmp_file();
void unlink_files(Str* names);
char* dir(char* name);
bool have_file(char* name);
FILE* open_input_file(char* fname);
FILE* open_output_file(char* fname);
char* read_file_contents(char* fname);

// string.c
void add_str(Str** strs, Str* str);
Str* new_str(char* data);
Str* copy_str(Str* src);
Str* append_strs(Str* former, Str* latter);
bool contain_str(Str* strs, char* c, int len);
Str* intersect_strs(Str* a, Str* b);
char* format(char* fmt, ...);
bool equal_to_str(char* s, char* other);
bool equal_to_n_chars(char* s, char* c, int n);
bool start_with(char* s, char* prefix);
bool start_with_any(char* s, ...);
bool start_with_insensitive(char* s, char* prefix);

// type.c
Type* new_void_type();
Type* new_bool_type();
Type* new_char_type();
Type* new_uchar_type();
Type* new_short_type();
Type* new_ushort_type();
Type* new_int_type();
Type* new_uint_type();
Type* new_long_type();
Type* new_ulong_type();
Type* new_float_type();
Type* new_double_type();
Type* new_ptr_type(Type* base);
Type* new_func_type(Type* return_type, Type* params, bool is_variadic);
Type* new_array_type(Type* base, int len);
Type* new_chars_type(int len);
Type* new_struct_type(int size, int alignment, Member* mems);
Type* new_union_type(int size, int alignment, Member* mems);
Type* copy_type(Type* src);
Type* copy_type_with_name(Type* src, char* name);
Type* copy_composite_type(Type* src, TypeKind kind);
Type* get_common_type(Type* a, Type* b);
Type* deref_ptr_type(Type* type);
Type* inherit_decl(Type* dst, Type* src);
bool is_pointable_type(Type* type);
bool is_int_type(Type* type);
bool is_float_type(Type* type);
bool is_numeric_type(Type* type);
bool is_composite_type(Type* type);

// unicode.c
int encode_to_utf8(char* dst, uint32_t code);
uint32_t decode_from_utf8(char** dst, char* src);
int encode_to_utf16(uint16_t* dst, uint32_t code);
