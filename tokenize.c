#include "cc.h"

static char* user_input;

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

static bool starting_with(char* p, char* q) {
  return memcmp(p, q, strlen(q)) == 0;
}

static bool identable1(char c) {
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_';
}

static bool identable2(char c) {
  return identable1(c) || ('0' <= c && c <= '9');
}

static bool equal_str(char* p, char* keyword) {
  return starting_with(p, keyword) && !identable2(p[strlen(keyword)]);
}

static bool consume_keyword(Token** tok, char** p) {
  static char* ks[] = {
      "if",   "else",  "for", "while", "return", "sizeof", "void",
      "char", "short", "int", "long",  "struct", "union",
  };
  int len = sizeof(ks) / sizeof(char*);
  for (int i = 0; i < len; i++) {
    if (equal_str(*p, ks[i])) {
      int klen = strlen(ks[i]);
      *tok = new_token(TK_RESERVED, *p, klen);
      *p += klen;
      return true;
    }
  }
  return false;
}

static void read_file() {
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
  FILE* stream = open_memstream(&user_input, &len);

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

  if (len == 0 || user_input[len - 1] != '\n') {
    fputc('\n', stream);
  }
  fputc('\0', stream);
  fclose(stream);
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

Token* tokenize() {
  read_file();
  char* p = user_input;

  Token head = {};
  Token* cur = &head;

  while (*p) {
    if (isspace(*p)) {
      p++;
      continue;
    }

    if (starting_with(p, "//")) {
      while (*p != '\n') {
        p++;
      }
      continue;
    }

    if (starting_with(p, "/*")) {
      char* close = strstr(p + 2, "*/");
      if (!close) {
        error_at(p, "unclosed block comment");
      }
      p = close + 2;
      continue;
    }

    if (consume_keyword(&cur->next, &p)) {
      cur = cur->next;
      continue;
    }

    if (starting_with(p, "==") || starting_with(p, "!=") ||
        starting_with(p, "<=") || starting_with(p, ">=") ||
        starting_with(p, "->")) {
      cur->next = new_token(TK_RESERVED, p, 2);
      cur = cur->next;
      p += 2;
      continue;
    }

    if (strchr("+-*/()<>=;{},.&[]", *p)) {
      cur->next = new_token(TK_RESERVED, p++, 1);
      cur = cur->next;
      continue;
    }

    if (identable1(*p)) {
      char* start = p;
      do {
        p++;
      } while (identable2(*p));
      cur->next = new_token(TK_IDENT, start, p - start);
      cur = cur->next;
      continue;
    }

    if (isdigit(*p)) {
      cur->next = new_token(TK_NUM, p, 0);
      cur->next->int_val = strtol(p, &p, 10);
      cur = cur->next;
      continue;
    }

    if (*p == '"') {
      char* start = p++;
      for (; *p != '"'; p++) {
        if (*p == '\n' || *p == '\0') {
          error_at(start, "unclosed string literal");
        }
        if (*p == '\\') {
          p++;
        }
      }
      char* end = p++;

      char* val = calloc(1, end - start);
      int i = 0;
      for (char* p = start + 1; p < end;) {
        if (*p == '\\') {
          p++;
          val[i++] = read_escaped_char(&p);
          continue;
        }
        val[i++] = *p++;
      }

      cur->next = new_token(TK_STR, start + 1, end - start - 1);
      cur->next->str_val = val;
      cur = cur->next;
      continue;
    }

    error_at(p, "invalid character");
  }

  cur->next = new_token(TK_EOF, p, 0);

  Token* tokens = head.next;
  add_line_number(tokens);
  return tokens;
}