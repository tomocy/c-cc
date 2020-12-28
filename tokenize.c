#include "cc.h"

static char* user_input;

static Token* new_token(TokenKind kind, char* loc, int len);

static void error_at(char* loc, char* fmt, ...);

static void read_file(char** content);
static void add_line_number(Token* token);

static bool consume_comment(char** c);
static bool consume_keyword(Token** dst, char** c);
static bool consume_punct(Token** dst, char** c);
static bool consume_ident(Token** dst, char** c);
static bool consume_number(Token** dst, char** c);
static bool consume_char(Token** dst, char** c);
static bool consume_str(Token** dst, char** c);

Token* tokenize() {
  read_file(&user_input);
  char* c = user_input;

  Token head = {};
  Token* cur = &head;

  while (*c) {
    if (isspace(*c)) {
      c++;
      continue;
    }

    if (consume_comment(&c)) {
      continue;
    }

    if (consume_keyword(&cur->next, &c)) {
      cur = cur->next;
      continue;
    }

    if (consume_punct(&cur->next, &c)) {
      cur = cur->next;
      continue;
    }

    if (consume_ident(&cur->next, &c)) {
      cur = cur->next;
      continue;
    }

    if (consume_number(&cur->next, &c)) {
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

    error_at(c, "invalid character");
  }

  cur->next = new_token(TK_EOF, c, 0);

  Token* tokens = head.next;
  add_line_number(tokens);
  return tokens;
}

bool equal_to_token(Token* token, char* s) {
  return memcmp(token->loc, s, token->len) == 0 && s[token->len] == '\0';
}

bool consume_token(Token** token, char* s) {
  if (!equal_to_token(*token, s)) {
    return false;
  }

  *token = (*token)->next;
  return true;
}

void expect_token(Token** token, char* s) {
  if (!consume_token(token, s)) {
    error_at((*token)->loc, "expected '%s'", s);
  }
}

int expect_num(Token** token) {
  if ((*token)->kind != TK_NUM) {
    error_token(*token, "expected a number");
  }

  int val = (*token)->int_val;
  *token = (*token)->next;
  return val;
}

static Token* new_token(TokenKind kind, char* loc, int len) {
  Token* tok = calloc(1, sizeof(Token));
  tok->kind = kind;
  tok->loc = loc;
  tok->len = len;
  return tok;
}

void error(char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

static void verror_at(int line, char* loc, char* fmt, va_list args) {
  char* start = loc;
  while (user_input < start && start[-1] != '\n') {
    start--;
  }
  char* end = loc;
  while (*end != '\n') {
    end++;
  }

  int indent = fprintf(stderr, "%s:%d ", input_filename, line);
  fprintf(stderr, "%.*s\n", (int)(end - start), start);

  int pos = loc - start + indent;
  fprintf(stderr, "%*s", pos, "");
  fprintf(stderr, "^ ");
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");

  exit(1);
}

static void error_at(char* loc, char* fmt, ...) {
  int line = 1;
  for (char* cur = user_input; cur < loc; cur++) {
    if (*cur == '\n') {
      line++;
    }
  }

  va_list args;
  va_start(args, fmt);
  verror_at(line, loc, fmt, args);
}

void error_token(Token* token, char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  verror_at(token->line, token->loc, fmt, args);
}

static void warn_at(int line, char* loc) {
  char* start = loc;
  while (user_input < start && start[-1] != '\n') {
    start--;
  }
  char* end = loc;
  while (*end != '\n') {
    end++;
  }

  int indent = fprintf(stderr, "%s:%d ", input_filename, line);
  fprintf(stderr, "%.*s\n", (int)(end - start), start);

  int pos = loc - start + indent;
  fprintf(stderr, "%*s", pos, "");
  fprintf(stderr, "^ ");
  fprintf(stderr, "\n");
}

void warn_token(Token* token) { warn_at(token->line, token->loc); }

static bool starting_with(char* c, char* prefix) {
  return memcmp(c, prefix, strlen(prefix)) == 0;
}

static bool is_identable1(char c) {
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_';
}

static bool is_identable2(char c) {
  return is_identable1(c) || ('0' <= c && c <= '9');
}

static void read_file(char** content) {
  FILE* f;
  if (strcmp(input_filename, "-") == 0) {
    f = stdin;
  } else {
    f = fopen(input_filename, "r");
    if (!f) {
      error("cannot open input file %s: %s", input_filename, strerror(errno));
    }
  }

  size_t len;
  FILE* stream = open_memstream(content, &len);

  for (;;) {
    char tmp[4096];
    int n = fread(tmp, 1, sizeof(tmp), f);
    if (n == 0) {
      break;
    }
    fwrite(tmp, 1, n, stream);
  }

  if (f != stdin) {
    fclose(f);
  }

  fflush(stream);

  if (len == 0 || (*content)[len - 1] != '\n') {
    fputc('\n', stream);
  }
  fputc('\0', stream);
  fclose(stream);
}

static void add_line_number(Token* token) {
  int line = 1;
  for (char* loc = user_input; *loc; loc++) {
    if (*loc == '\n') {
      line++;
      continue;
    }

    if (loc == token->loc) {
      token->line = line;
      token = token->next;
      continue;
    }
  }
}

static bool consume_comment(char** c) {
  if (starting_with(*c, "//")) {
    while (**c != '\n') {
      (*c)++;
    }
    return true;
  }

  if (starting_with(*c, "/*")) {
    char* close = strstr(*c + 2, "*/");
    if (!close) {
      error_at(*c, "unclosed block comment");
    }
    *c = close + 2;
    return true;
  }

  return false;
}

static bool can_be_as_keyward(char* kw, char* c) {
  return starting_with(c, kw) && !is_identable2(c[strlen(kw)]);
}

static bool consume_keyword(Token** dst, char** c) {
  static char* kws[] = {
      "if",     "else",  "for",  "while",   "return", "sizeof",
      "void",   "_Bool", "char", "short",   "int",    "long",
      "struct", "union", "enum", "typedef", "static",
  };
  static int klen = sizeof(kws) / sizeof(char*);

  for (int i = 0; i < klen; i++) {
    if (!can_be_as_keyward(kws[i], *c)) {
      continue;
    }

    int len = strlen(kws[i]);
    *dst = new_token(TK_RESERVED, *c, len);
    *c += len;
    return true;
  }

  return false;
}

static bool consume_duo_punct(Token** dst, char** c) {
  static char* puncts[] = {
      "==", "!=", "<=", ">=", "->", "+=", "-=", "*=", "/=", "++", "--"};
  static int plen = sizeof(puncts) / sizeof(char*);

  for (int i = 0; i < plen; i++) {
    if (!starting_with(*c, puncts[i])) {
      continue;
    }

    int len = strlen(puncts[i]);
    *dst = new_token(TK_RESERVED, *c, len);
    *c += len;
    return true;
  }

  return false;
}

static bool consume_mono_punct(Token** dst, char** c) {
  if (!strchr("+-*/()<>=;{},.&[]", **c)) {
    return false;
  }

  *dst = new_token(TK_RESERVED, *c, 1);
  (*c)++;
  return true;
}

static bool consume_punct(Token** dst, char** c) {
  if (consume_duo_punct(dst, c)) {
    return true;
  }

  if (consume_mono_punct(dst, c)) {
    return true;
  }

  return false;
}

static bool consume_ident(Token** dst, char** c) {
  if (!is_identable1(**c)) {
    return false;
  }

  char* start = *c;
  do {
    (*c)++;
  } while (is_identable2(**c));

  *dst = new_token(TK_IDENT, start, *c - start);
  return true;
}

static bool consume_number(Token** dst, char** c) {
  if (!isdigit(**c)) {
    return false;
  }

  char* start = *c;

  int base = 10;
  if (strncasecmp(*c, "0x", 2) == 0 && isalnum((*c)[2])) {
    *c += 2;
    base = 16;
  } else if (strncasecmp(*c, "0b", 2) == 0 && isalnum((*c)[2])) {
    *c += 2;
    base = 2;
  } else if (**c == '0') {
    base = 8;
  }

  long val = strtoul(*c, c, base);
  if (isalnum(**c)) {
    error_at(*c, "invalid digit");
  }

  Token* token = new_token(TK_NUM, start, *c - start);
  token->int_val = val;
  *dst = token;
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
      error_at(*c, "expected a hex escape sequence");
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
  if (**c != '\'') {
    return false;
  }

  char* start = (*c)++;

  if (**c == '\n' || **c == '\0') {
    error_at(start, "unclosed string literal");
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
    error_at(start, "unclosed string literal");
  }

  Token* token = new_token(TK_NUM, start + 1, end - start - 1);
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
      error_at(start, "unclosed string literal");
    }
    if (**c == '\\') {
      (*c)++;
    }
  }
  char* end = (*c)++;

  char* val = calloc(1, end - start);
  int i = 0;
  for (char* c = start + 1; c < end;) {
    if (*c == '\\') {
      c++;
      val[i++] = read_escaped_char(&c);
      continue;
    }
    val[i++] = *c++;
  }

  Token* token = new_token(TK_STR, start + 1, end - start - 1);
  token->str_val = val;
  *dst = token;
  return true;
}