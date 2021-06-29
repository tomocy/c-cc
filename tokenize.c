#include "cc.h"

File* files;
static File* current_file;
static bool is_bol;
static bool was_space;

static File* read_file(char* fname);
static void canonicalize_newlines(char* contents);
static void remove_backslach_newlines(char* contents);
static void add_line_number(Token* token);

static Token* new_token(TokenKind kind, char* loc, int len);
static Token* new_eof_token();

static bool consume_comment(char** c);
static bool consume_punct(Token** dst, char** c);
static bool consume_ident(Token** dst, char** c);
static bool consume_pp_num(Token** dst, char** c);
static bool consume_char(Token** dst, char** c);
static bool consume_str(Token** dst, char** c);

Token* tokenize(char* input_filename) {
  File* file = read_file(input_filename);
  canonicalize_newlines(file->contents);
  remove_backslach_newlines(file->contents);

  return tokenize_in(file);
}

Token* tokenize_in(File* file) {
  current_file = file;

  char* c = file->contents;

  Token head = {};
  Token* cur = &head;
  is_bol = true;
  was_space = false;
  while (*c) {
    if (*c == '\n') {
      is_bol = true;
      c++;
      continue;
    }

    if (isspace(*c)) {
      was_space = true;
      c++;
      continue;
    }

    if (consume_comment(&c)) {
      continue;
    }

    if (consume_pp_num(&cur->next, &c)) {
      cur = cur->next;
      continue;
    }

    if (consume_char(&cur->next, &c)) {
      cur = cur->next;
      continue;
    }

    if (consume_str(&cur->next, &c)) {
      cur = cur->next;
      continue;
    }

    if (consume_ident(&cur->next, &c)) {
      cur = cur->next;
      continue;
    }

    if (consume_punct(&cur->next, &c)) {
      cur = cur->next;
      continue;
    }

    error_at(current_file, c, "invalid character");
  }

  cur->next = new_eof_token();
  current_file = NULL;

  Token* tokens = head.next;
  add_line_number(tokens);
  return tokens;
}

static void add_file(File* file) {
  file->next = files;
  files = file;
}

static File* create_file(int index, char* name, char* contents) {
  File* file = new_file(index, name, contents);
  add_file(file);
  return file;
}

static File* read_file(char* fname) {
  static int index = 0;
  return create_file(++index, fname, read_file_contents(fname));
}

static void canonicalize_newlines(char* contents) {
  int peeked = 0;
  int i = 0;

  while (contents[peeked]) {
    if (contents[peeked] == '\r' && contents[peeked + 1] == '\n') {
      peeked += 2;
      contents[i++] = '\n';
      continue;
    }
    if (contents[peeked] == '\r') {
      peeked++;
      contents[i++] = '\n';
      continue;
    }

    contents[i++] = contents[peeked++];
  }

  contents[i] = '\0';
}

static void remove_backslach_newlines(char* contents) {
  int peeked = 0;
  int i = 0;

  while (contents[peeked]) {
    if (contents[peeked] == '\\' && contents[peeked + 1] == '\n') {
      peeked += 2;
      continue;
    }

    contents[i++] = contents[peeked++];
  }

  contents[i] = '\0';
}

static void add_line_number(Token* token) {
  int line = 1;
  for (char* c = token->file->contents; *c; c++) {
    if (*c == '\n') {
      line++;
      continue;
    }

    if (c == token->loc) {
      token->line = line;
      token = token->next;
      continue;
    }
  }
}

bool equal_to_token(Token* token, char* s) {
  return token && equal_to_n_chars(s, token->loc, token->len);
}

bool equal_to_tokens(Token* token, int len, ...) {
  va_list ss;
  va_start(ss, len);
  Token* cur = token;
  for (int i = 0; i < len; i++) {
    if (!equal_to_token(cur, va_arg(ss, char*))) {
      return false;
    }
    if (cur) {
      cur = cur->next;
    }
  }
  va_end(ss);

  return true;
}

bool equal_to_ident_token(Token* token, char* s) {
  return token->kind == TK_IDENT && equal_to_token(token, s);
}

bool consume_token(Token** token, char* s) {
  if (!equal_to_token(*token, s)) {
    return false;
  }

  *token = (*token)->next;
  return true;
}

Token* expect_token(Token** tokens, char* s) {
  Token* start = *tokens;
  if (!consume_token(tokens, s)) {
    error_at((*tokens)->file, (*tokens)->loc, "expected '%s'", s);
  }

  return start;
}

Token* expect_tokens(Token** tokens, int len, ...) {
  Token* start = *tokens;

  va_list ss;
  va_start(ss, len);
  for (int i = 0; i < len; i++) {
    expect_token(tokens, va_arg(ss, char*));
  }
  va_end(ss);

  return start;
}

Token* expect_ident_token(Token** tokens) {
  if ((*tokens)->kind != TK_IDENT) {
    error_token(*tokens, "expected an ident");
  }

  Token* ident = *tokens;
  *tokens = (*tokens)->next;
  return ident;
}

Token* new_token_in(TokenKind kind, File* file, char* loc, int len) {
  Token* token = calloc(1, sizeof(Token));
  token->kind = kind;
  token->file = file;
  token->loc = loc;
  token->len = len;
  return token;
}

Token* new_eof_token_in(File* file) {
  return new_token_in(TK_EOF, file, NULL, 0);
}

static Token* new_token(TokenKind kind, char* loc, int len) {
  Token* token = new_token_in(kind, current_file, loc, len);
  token->is_bol = is_bol;
  is_bol = false;
  token->has_leading_space = was_space;
  was_space = false;
  return token;
}

static Token* new_eof_token() {
  return new_token(TK_EOF, NULL, 0);
}

Token* copy_token(Token* src) {
  Token* token = calloc(1, sizeof(Token));
  *token = *src;
  token->next = NULL;
  return token;
}

Token* copy_tokens(Token* src) {
  Token head = {};
  Token* cur = &head;
  for (Token* token = src; token; token = token->next) {
    cur = cur->next = copy_token(token);
  }

  return head.next;
}

void print_tokens(char* output_filename, Token* tokens) {
  FILE* file = open_output_file(output_filename);

  int line = 0;
  for (Token* token = tokens; token && token->kind != TK_EOF; token = token->next) {
    if (token->is_bol) {
      if (line++ >= 1) {
        fprintf(file, "\n");
      }
    }
    if (token->has_leading_space) {
      fprintf(file, " ");
    }
    fprintf(file, "%.*s", token->len, token->loc);
  }
  fprintf(file, "\n");

  fclose(file);
}

void error_token(Token* token, char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vprint_at(token->file, token->loc, fmt, args);
  va_end(args);
  exit(1);
}

void warn_token(Token* token, char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vprint_at(token->file, token->loc, fmt, args);
  va_end(args);
}

static bool can_be_ident(char c) {
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_';
}

static bool can_be_ident2(char c) {
  return can_be_ident(c) || ('0' <= c && c <= '9');
}

static bool consume_comment(char** c) {
  if (start_with(*c, "//")) {
    while (**c != '\n') {
      (*c)++;
    }
    return true;
  }

  if (start_with(*c, "/*")) {
    char* close = strstr(*c + 2, "*/");
    if (!close) {
      error_at(current_file, *c, "unclosed block comment");
    }
    *c = close + 2;
    return true;
  }

  return false;
}

bool can_be_keyword(char* c, int len) {
  return !can_be_ident2(c[len]);
}

static bool consume_punct(Token** dst, char** c) {
  static char* puncts[] = {
    "<<=",
    ">>=",
    "...",
    "==",
    "!=",
    "<=",
    ">=",
    "->",
    "+=",
    "-=",
    "*=",
    "/=",
    "%=",
    "|=",
    "^=",
    "&=",
    "||",
    "&&",
    "++",
    "--",
    "<<",
    ">>",
    "##",
    "+",
    "-",
    "*",
    "/",
    "%",
    "(",
    ")",
    "<",
    ">",
    "=",
    ":",
    ";",
    "{",
    "}",
    ",",
    ".",
    "&",
    "|",
    "^",
    "[",
    "]",
    "!",
    "~",
    "?",
    "#",
    "`",
  };
  static int plen = sizeof(puncts) / sizeof(char*);

  for (int i = 0; i < plen; i++) {
    if (!start_with(*c, puncts[i])) {
      continue;
    }

    int len = strlen(puncts[i]);
    *dst = new_token(TK_RESERVED, *c, len);
    *c += len;
    return true;
  }

  return false;
}

static bool consume_ident(Token** dst, char** c) {
  if (!can_be_ident(**c)) {
    return false;
  }

  char* start = *c;
  do {
    (*c)++;
  } while (can_be_ident2(**c));

  *dst = new_token(TK_IDENT, start, *c - start);
  return true;
}

// The definition of numeric literal at preprocessing stage is more relaxed
// than the one at later stage, so tokenize numberic literal as "pp-number" tokens
// in this stage and convert them to numbers after preprocessing.
static bool consume_pp_num(Token** dst, char** c) {
  if (!isdigit(**c) && !(**c == '.' && isdigit((*c)[1]))) {
    return false;
  }

  char* start = *c;
  while (**c) {
    if ((*c)[1] && strchr("eEpP", **c) && strchr("+-", (*c)[1])) {
      *c += 2;
      continue;
    }
    if (isalnum(**c) || **c == '.') {
      (*c)++;
      continue;
    }

    break;
  }

  *dst = new_token(TK_PP_NUM, start, *c - start);
  return true;
}

static int hex_digit(char c) {
  if ('0' <= c && c <= '9') {
    return c - '0';
  }

  if ('a' <= c && c <= 'f') {
    return c - 'a' + 10;
  }

  if ('A' <= c && c <= 'Z') {
    return c - 'A' + 10;
  }

  return c;
}

static int read_escaped_char(char** c) {
  if ('0' <= **c && **c <= '7') {
    int digit = *(*c)++ - '0';
    if ('0' <= **c && **c <= '7') {
      digit = (digit << 3) + *(*c)++ - '0';
      if ('0' <= **c && **c <= '7') {
        digit = (digit << 3) + *(*c)++ - '0';
      }
    }

    return digit;
  }

  if (**c == 'x') {
    (*c)++;
    if (!isxdigit(**c)) {
      error_at(current_file, *c, "expected a hex escape sequence");
    }

    int digit = 0;
    for (; isxdigit(**c); (*c)++) {
      digit = (digit << 4) + hex_digit(**c);
    }

    return digit;
  }

  char escaped = *(*c)++;
  switch (escaped) {
    case 'a':
      return '\a';
    case 'b':
      return '\b';
    case 't':
      return '\t';
    case 'n':
      return '\n';
    case 'v':
      return '\v';
    case 'f':
      return '\f';
    case 'r':
      return '\r';
    case 'e':
      return 27;
    default:
      return escaped;
  }
}

static bool consume_char(Token** dst, char** c) {
  if (**c != '\'' && !start_with(*c, "L'")) {
    return false;
  }
  if (**c == 'L') {
    (*c)++;
  }

  char* start = (*c)++;

  if (**c == '\n' || **c == '\0') {
    error_at(current_file, start, "unclosed string literal");
  }

  char read;
  if (**c == '\\') {
    (*c)++;
    read = read_escaped_char(c);
  } else {
    read = **c;
    (*c)++;
  }

  char* end = (*c)++;
  if (*end != '\'') {
    error_at(current_file, start, "unclosed string literal");
  }

  Token* token = new_token(TK_NUM, start, end - start + 1);
  token->type = new_int_type();
  token->int_val = read;
  *dst = token;
  return true;
}

static bool consume_str(Token** dst, char** c) {
  if (**c != '"') {
    return false;
  }

  char* start = (*c)++;
  for (; **c != '"'; (*c)++) {
    if (**c == '\n' || **c == '\0') {
      error_at(current_file, start, "unclosed string literal");
    }
    if (**c == '\\') {
      (*c)++;
    }
  }
  char* end = (*c)++;

  char* val = calloc(1, end - start);
  int val_len = 0;
  for (char* c = start + 1; c < end;) {
    if (*c == '\\') {
      c++;
      val[val_len++] = read_escaped_char(&c);
      continue;
    }
    val[val_len++] = *c++;
  }

  Token* token = new_token(TK_STR, start, end - start + 1);
  token->str_val = val;
  token->str_val_len = val_len;
  *dst = token;
  return true;
}