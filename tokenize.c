#include "cc.h"

static char* user_input;
static bool is_bol;

static Token* new_token(TokenKind kind, char* loc, int len);

static void read_file(char** content);
static void add_line_number(Token* token);

static bool consume_comment(char** c);
static bool consume_keyword(Token** dst, char** c);
static bool consume_punct(Token** dst, char** c);
static bool consume_ident(Token** dst, char** c);
static bool consume_number(Token** dst, char** c);
static bool consume_char(Token** dst, char** c);
static bool consume_str(Token** dst, char** c);

Token* tokenize(void) {
  read_file(&user_input);
  char* c = user_input;

  Token head = {};
  Token* cur = &head;
  is_bol = true;
  while (*c) {
    if (*c == '\n') {
      is_bol = true;
      c++;
      continue;
    }

    if (isspace(*c)) {
      c++;
      continue;
    }

    if (consume_comment(&c)) {
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

    if (consume_keyword(&cur->next, &c)) {
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

    error_at(input_filename, user_input, c, "invalid character");
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
    error_at(input_filename, user_input, (*token)->loc, "expected '%s'", s);
  }
}

static Token* new_token(TokenKind kind, char* loc, int len) {
  Token* tok = calloc(1, sizeof(Token));
  tok->kind = kind;
  tok->loc = loc;
  tok->len = len;
  tok->is_bol = is_bol;
  is_bol = false;
  return tok;
}

void error_token(Token* token, char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  verror_at(input_filename, user_input, token->loc, fmt, args);
}

void warn_token(Token* token) {
  warn_at(input_filename, user_input, token->loc);
}

static bool isbdigit(char c) {
  return c == '0' || c == '1';
}

static bool can_be_ident(char c) {
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_';
}

static bool can_be_ident2(char c) {
  return can_be_ident(c) || ('0' <= c && c <= '9');
}

static void read_file(char** contents) {
  FILE* f;
  if (equal_to_str(input_filename, "-")) {
    f = stdin;
  } else {
    f = fopen(input_filename, "r");
    if (!f) {
      error("cannot open input file %s: %s", input_filename, strerror(errno));
    }
  }

  size_t len;
  FILE* stream = open_memstream(contents, &len);

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

  if (len == 0 || (*contents)[len - 1] != '\n') {
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
  if (start_with(*c, "//")) {
    while (**c != '\n') {
      (*c)++;
    }
    return true;
  }

  if (start_with(*c, "/*")) {
    char* close = strstr(*c + 2, "*/");
    if (!close) {
      error_at(input_filename, user_input, *c, "unclosed block comment");
    }
    *c = close + 2;
    return true;
  }

  return false;
}

static bool can_be_keyword(char* s, char* keyword) {
  return start_with(s, keyword) && !can_be_ident2(s[strlen(keyword)]);
}

static bool consume_keyword(Token** dst, char** c) {
  static char* kws[] = {
    "if",
    "else",
    "for",
    "while",
    "do",
    "return",
    "sizeof",
    "void",
    "_Bool",
    "char",
    "short",
    "int",
    "long",
    "float",
    "double",
    "struct",
    "union",
    "enum",
    "typedef",
    "static",
    "goto",
    "break",
    "continue",
    "switch",
    "case",
    "default",
    "_Alignof",
    "_Alignas",
    "signed",
    "unsigned",
    "const",
    "volatile",
    "auto",
    "register",
    "restrict",
    "__restrict",
    "__restrict__",
    "_Noreturn",
  };
  static int klen = sizeof(kws) / sizeof(char*);

  for (int i = 0; i < klen; i++) {
    if (!can_be_keyword(*c, kws[i])) {
      continue;
    }

    int len = strlen(kws[i]);
    *dst = new_token(TK_RESERVED, *c, len);
    *c += len;
    return true;
  }

  return false;
}

static bool consume_punct(Token** dst, char** c) {
  static char* puncts[] = {
    "<<=",
    ">>=",
    "...",  // tri
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
    ">>",  // duo
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

// NOLINTNEXTLINE
static Token* read_int(char** c) {
  char* start = *c;

  int base = 10;
  if (start_with_insensitive(*c, "0x") && isxdigit((*c)[2])) {
    *c += 2;
    base = 16;
  } else if (start_with_insensitive(*c, "0b") && isbdigit((*c)[2])) {
    *c += 2;
    base = 2;
  } else if (**c == '0') {
    base = 8;
  }

  int64_t val = strtoul(*c, c, base);

  bool l = false;
  bool u = false;
  if (start_with(*c, "LLU") || start_with(*c, "LLu") || start_with(*c, "llU") || start_with(*c, "llu")
      || start_with(*c, "ULL") || start_with(*c, "Ull") || start_with(*c, "uLL") || start_with(*c, "ull")) {
    *c += 3;
    l = u = true;
  } else if (start_with_insensitive(*c, "LU") || start_with_insensitive(*c, "UL")) {
    *c += 2;
    l = u = true;
  } else if (start_with(*c, "LL") || start_with_insensitive(*c, "ll")) {
    *c += 2;
    l = true;
  } else if (start_with_insensitive(*c, "L")) {
    (*c)++;
    l = true;
  } else if (start_with_insensitive(*c, "U")) {
    (*c)++;
    u = true;
  }

  Type* type = NULL;
  if (base == 10) {
    if (l && u) {
      type = new_ulong_type();
    } else if (l) {
      type = new_long_type();
    } else if (u) {
      type = (val >> 32) ? new_ulong_type() : new_uint_type();
    } else if (val >> 31) {
      type = new_long_type();
    } else {
      type = new_int_type();
    }
  } else {
    if (l && u) {
      type = new_ulong_type();
    } else if (l) {
      type = (val >> 63) ? new_ulong_type() : new_long_type();
    } else if (u) {
      type = (val >> 32) ? new_ulong_type() : new_uint_type();
    } else if (val >> 63) {
      type = new_ulong_type();
    } else if (val >> 32) {
      type = new_long_type();
    } else if (val >> 31) {
      type = new_uint_type();
    } else {
      type = new_int_type();
    }
  }

  Token* token = new_token(TK_NUM, start, *c - start);
  token->type = type;
  token->int_val = val;
  return token;
}

static Token* read_float(char** c) {
  char* start = *c;

  double val = strtod(start, c);

  Type* type = NULL;
  if (start_with_insensitive(*c, "F")) {
    type = new_float_type();
    (*c)++;
  } else if (start_with_insensitive(*c, "L")) {
    type = new_double_type();
    (*c)++;
  } else {
    type = new_double_type();
  }

  Token* token = new_token(TK_NUM, start, *c - start);
  token->type = type;
  token->float_val = val;
  return token;
}

// NOLINTNEXTLINE
static bool consume_number(Token** dst, char** c) {
  if (!isdigit(**c) && !(**c == '.' && isdigit((*c)[1]))) {
    return false;
  }

  char* peeked = *c;
  Token* token = read_int(&peeked);
  if (strchr(".EeFf", *peeked) == 0) {
    *c = peeked;
    *dst = token;
    return true;
  }

  token = read_float(c);
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
      error_at(input_filename, user_input, *c, "expected a hex escape sequence");
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
    error_at(input_filename, user_input, start, "unclosed string literal");
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
    error_at(input_filename, user_input, start, "unclosed string literal");
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
      error_at(input_filename, user_input, start, "unclosed string literal");
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