#include "cc.h"

File* files;
static File* current_file;
static bool is_bol;
static bool was_space;

static File* read_file(char* fname);
static void canonicalize_newlines(char* contents);
static void remove_backslach_newlines(char* contents);
static void convert_universal_chars(char* contents);
static void add_line_number(Token* token);

static Token* new_token(TokenKind kind, char* loc, int len);
static Token* new_eof_token();

static bool consume_comment(char** c);
static bool consume_punct(Token** dst, char** c);
static bool consume_ident(Token** dst, char** c);
static bool consume_pp_num(Token** dst, char** c);
static bool consume_char(Token** dst, char** c);
static bool consume_str(Token** dst, char** c);

Token* tokenize_all(Str* fnames) {
  Token* tokens = NULL;
  for (Str* fname = fnames; fname; fname = fname->next) {
    tokens = append_tokens(tokens, tokenize(fname->data));
  }

  return tokens;
}

Token* tokenize(char* input_filename) {
  File* file = read_file(input_filename);

  // Skip UTF-8 BOM
  if (memcmp(file->contents, "\xef\xbb\xbf", 3) == 0) {
    file->contents += 3;
  }
  canonicalize_newlines(file->contents);
  remove_backslach_newlines(file->contents);
  convert_universal_chars(file->contents);

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
  int removed = 0;

  while (contents[peeked]) {
    if (contents[peeked] == '\\' && contents[peeked + 1] == '\n') {
      peeked += 2;
      removed++;
      continue;
    }

    if (contents[peeked] == '\n') {
      contents[i++] = contents[peeked++];
      // Append new lines the number of the removed ones times so that the logical number of lines
      // finally matches the phisical ones at the end.
      for (; removed > 0; removed--) {
        contents[i++] = '\n';
      }
      continue;
    }

    contents[i++] = contents[peeked++];
  }

  for (; removed > 0; removed--) {
    contents[i++] = '\n';
  }
  contents[i] = '\0';
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

static uint32_t read_universal_char(char* c, int len) {
  uint32_t code = 0;
  for (int i = 0; i < len; i++) {
    if (!isxdigit(c[i])) {
      return 0;
    }

    code = (code << 4) | hex_digit(c[i]);
  }

  return code;
}

static void convert_universal_chars(char* contents) {
  int peeked = 0;
  int i = 0;

  while (contents[peeked]) {
    if (start_with(&contents[peeked], "\\u")) {
      uint32_t code = read_universal_char(&contents[peeked + 2], 4);
      if (code) {
        peeked += 6;
        i += encode_to_utf8(&contents[i], code);
      } else {
        contents[i++] = contents[peeked++];
      }
      continue;
    }

    if (start_with(&contents[peeked], "\\U")) {
      uint32_t code = read_universal_char(&contents[peeked + 2], 8);
      if (code) {
        peeked += 10;
        i += encode_to_utf8(&contents[i], code);
      } else {
        contents[i++] = contents[peeked++];
      }
      continue;
    }

    if (start_with(&contents[peeked], "\\")) {
      contents[i++] = contents[peeked++];
      contents[i++] = contents[peeked++];
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

bool equal_to_any_token(Token* token, ...) {
  va_list ss;
  va_start(ss, token);
  for (char* s = va_arg(ss, char*); s; s = va_arg(ss, char*)) {
    if (equal_to_token(token, s)) {
      return true;
    }
  }

  return false;
}

bool equal_to_tokens(Token* token, ...) {
  va_list ss;
  va_start(ss, token);
  Token* cur = token;
  for (char* s = va_arg(ss, char*); s; s = va_arg(ss, char*)) {
    if (!equal_to_token(cur, s)) {
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

bool consume_token(Token** tokens, char* s) {
  if (!equal_to_token(*tokens, s)) {
    return false;
  }

  *tokens = (*tokens)->next;
  return true;
}

Token* expect_token(Token** tokens, char* s) {
  Token* start = *tokens;
  if (!consume_token(tokens, s)) {
    error_token(*tokens, "expected '%s'", s);
  }

  return start;
}

Token* expect_tokens(Token** tokens, ...) {
  Token* start = *tokens;

  va_list ss;
  va_start(ss, tokens);
  for (char* s = va_arg(ss, char*); s; s = va_arg(ss, char*)) {
    expect_token(tokens, s);
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

Token* append_tokens(Token* former, Token* latter) {
  Token head = {};
  Token* cur = &head;
  for (Token* token = former; token && token->kind != TK_EOF; token = token->next) {
    cur = cur->next = copy_token(token);
  }
  cur->next = latter;

  return head.next;
}

static bool is_std_include_file(char* fname) {
  for (Str* path = std_include_paths.head.next; path; path = path->next) {
    if (start_with(fname, path->data)) {
      return true;
    }
  }

  return false;
}

static void print_dep(FILE* dst, File* file, bool with_std) {
  if (!file) {
    return;
  }

  print_dep(dst, file->next, with_std);

  if (!with_std && is_std_include_file(file->name)) {
    return;
  }

  fprintf(dst, " \\\n  %s", file->name);
}

static void print_header_dep(FILE* dst, File* file, bool with_std) {
  if (!file) {
    return;
  }

  print_header_dep(dst, file->next, with_std);

  if (!end_with(file->name, ".h")) {
    return;
  }
  if (!with_std && is_std_include_file(file->name)) {
    return;
  }

  fprintf(dst, "%s:\n\n", file->name);
}

void print_deps(char* output_filename, char* target, bool with_header_deps, bool with_std) {
  FILE* dst = open_output_file(output_filename);

  fprintf(dst, "%s:", target);
  print_dep(dst, files, with_std);
  fprintf(dst, "\n\n");

  if (with_header_deps) {
    print_header_dep(dst, files, with_std);
  }

  fclose(dst);
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
  vprint_at(token->file, token->line, token->loc, fmt, args);
  va_end(args);
  exit(1);
}

void warn_token(Token* token, char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vprint_at(token->file, token->line, token->loc, fmt, args);
  va_end(args);
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

  if (ispunct(**c)) {
    *dst = new_token(TK_RESERVED, *c, 1);
    (*c)++;
    return true;
  }

  return false;
}

static bool consume_ident(Token** dst, char** c) {
  char* peeked = *c;
  if (!can_be_ident(decode_from_utf8(&peeked, peeked))) {
    return false;
  }

  char* start = *c;
  *c = peeked;

  for (uint32_t code = decode_from_utf8(&peeked, peeked); can_be_ident2(code);
       code = decode_from_utf8(&peeked, peeked)) {
    *c = peeked;
  }

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
  if (**c != '\'' && !start_with_any(*c, "u'", "U'", "L'", NULL)) {
    return false;
  }

  char* start = *c;

  CharKind kind = CHAR_VANILLA;
  if (**c == 'u') {
    kind = CHAR_UTF16;
    (*c)++;
  } else if (**c == 'U') {
    kind = CHAR_UTF32;
    (*c)++;
  } else if (**c == 'L') {
    kind = CHAR_WIDE;
    (*c)++;
  }

  (*c)++;  // for the opening quote
  if (**c == '\n' || **c == '\0') {
    error_at(current_file, start, "unclosed char literal");
  }

  int read;
  if (**c == '\\') {
    (*c)++;
    read = read_escaped_char(c);
  } else {
    read = decode_from_utf8(c, *c);
  }

  // Tokenize char literals relaxedly so that something such as a message
  // quoted with single quotes can be used in the preprocess stage.
  char* end = strchr(*c, '\'');
  if (!end) {
    error_at(current_file, start, "unclosed char literal");
  }
  *c = end + 1;

  Token* token = new_token(TK_NUM, start, end - start + 1);
  token->char_kind = kind;
  switch (kind) {
    case CHAR_UTF16:
      token->type = new_ushort_type();
      token->int_val = 0xFFFF & read;
      break;
    case CHAR_UTF32:
      token->type = new_uint_type();
      token->int_val = read;
      break;
    case CHAR_WIDE:
      token->type = new_int_type();
      token->int_val = read;
      break;
    default:
      token->type = new_int_type();
      token->int_val = (char)read;
      break;
  }
  *dst = token;
  return true;
}

static Token* read_vanilla_str_literal(char* start, char* opening, char* closing) {
  char* val = calloc(closing - opening, 1);
  int len = 0;
  for (char* c = opening + 1; c < closing;) {
    if (*c == '\\') {
      c++;
      val[len++] = read_escaped_char(&c);
      continue;
    }
    val[len++] = *c++;
  }

  Token* token = new_token(TK_STR, start, closing - start + 1);
  token->type = new_array_type(new_char_type(), len + 1);
  token->str_val = val;
  return token;
}

static Token* read_utf16_str_literal(char* start, char* opening, char* closing) {
  uint16_t* val = calloc(closing - opening, 2);
  int len = 0;
  for (char* c = opening + 1; c < closing;) {
    if (*c == '\\') {
      c++;
      val[len++] = read_escaped_char(&c);
      continue;
    }

    uint32_t code = decode_from_utf8(&c, c);
    len += encode_to_utf16(&val[len], code) / 2;
  }

  Token* token = new_token(TK_STR, start, closing - start + 1);
  token->type = new_array_type(new_ushort_type(), len + 1);
  token->str_val = (char*)val;
  return token;
}

static Token* read_utf32_str_literal(Type* type, char* start, char* opening, char* closing) {
  uint32_t* val = calloc(closing - opening, 4);
  int len = 0;
  for (char* c = opening + 1; c < closing;) {
    if (*c == '\\') {
      c++;
      val[len++] = read_escaped_char(&c);
      continue;
    }

    val[len++] = decode_from_utf8(&c, c);
  }

  Token* token = new_token(TK_STR, start, closing - start + 1);
  token->type = new_array_type(type, len + 1);
  token->str_val = (char*)val;
  return token;
}

static Token* read_str_literal(CharKind kind, char* start, char* opening, char* closing) {
  Token* token = NULL;
  switch (kind) {
    case CHAR_UTF16:
      token = read_utf16_str_literal(start, opening, closing);
      break;
    case CHAR_UTF32:
      token = read_utf32_str_literal(new_uint_type(), start, opening, closing);
      break;
    case CHAR_WIDE:
      token = read_utf32_str_literal(new_int_type(), start, opening, closing);
      break;
    default:
      token = read_vanilla_str_literal(start, opening, closing);
      break;
  }
  token->char_kind = kind;

  return token;
}

Token* read_str_literal_in(File* file, CharKind kind, char* start, char* opening, char* closing) {
  Token* token = read_str_literal(kind, start, opening, closing);
  token->file = file;
  return token;
}

static bool consume_str(Token** dst, char** c) {
  if (**c != '"' && !start_with_any(*c, "u8\"", "u\"", "U\"", "L\"", NULL)) {
    return false;
  }

  char* start = *c;

  CharKind kind = CHAR_VANILLA;
  if (start_with(*c, "u8\"")) {
    kind = CHAR_UTF8;
    *c += 2;
  } else if (**c == 'u') {
    kind = CHAR_UTF16;
    (*c)++;
  } else if (**c == 'U') {
    kind = CHAR_UTF32;
    (*c)++;
  } else if (**c == 'L') {
    kind = CHAR_WIDE;
    (*c)++;
  }

  char* opening = (*c)++;  // for the opning "
  for (; **c != '"'; (*c)++) {
    if (**c == '\n' || **c == '\0') {
      error_at(current_file, start, "unclosed string literal");
    }
    if (**c == '\\') {
      (*c)++;
    }
  }
  char* end = (*c)++;

  *dst = read_str_literal(kind, start, opening, end);

  return true;
}