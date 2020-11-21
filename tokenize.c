#include "cc.h"

char* filename;
char* user_input;

Token* token;

void error(char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

static void verror_at(char* loc, char* fmt, va_list args) {
  char* start = loc;
  while (user_input < start && start[-1] != '\n') {
    start--;
  }
  char* end = loc;
  while (*end != '\n') {
    end++;
  }

  int line_num = 1;
  for (char* p = user_input; p < start; p++) {
    if (*p == '\n') {
      line_num++;
    }
  }

  int indent = fprintf(stderr, "%s:%d ", filename, line_num);
  fprintf(stderr, "%.*s\n", (int)(end - start), start);

  int pos = loc - start + indent;
  fprintf(stderr, "%*s", pos, "");
  fprintf(stderr, "^ ");
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");

  exit(1);
}

static void error_at(char* loc, char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  verror_at(loc, fmt, args);
}

void error_tok(Token* tok, char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  verror_at(tok->loc, fmt, args);
}

bool equal(Token* tok, char* op) {
  return tok->kind == TK_RESERVED && memcmp(tok->loc, op, tok->len) == 0 &&
         op[tok->len] == '\0';
}

bool consume(char* op) {
  if (!equal(token, op)) {
    return false;
  }
  token = token->next;
  return true;
}

void expect(char* op) {
  if (!equal(token, op)) {
    error_at(token->loc, "expected '%s'", op);
  }
  token = token->next;
}

int expect_num() {
  if (token->kind != TK_NUM) {
    error_at(token->loc, "expected a number");
  }

  int val = token->val;
  token = token->next;
  return val;
}

bool at_eof() { return token->kind == TK_EOF; }

static Token* new_token(TokenKind kind, char* loc, int len) {
  Token* tok = calloc(1, sizeof(Token));
  tok->kind = kind;
  tok->loc = loc;
  tok->len = len;
  return tok;
}

static bool startswith(char* p, char* q) {
  return memcmp(p, q, strlen(q)) == 0;
}

static bool is_identable1(char c) {
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_';
}

static bool is_identable2(char c) {
  return is_identable1(c) || ('0' <= c && c <= '9');
}

static bool equal_str(char* p, char* keyword) {
  return startswith(p, keyword) && !is_identable2(p[strlen(keyword)]);
}

static bool consume_keyword(Token** tok, char** p) {
  static char* ks[] = {
      "if", "else", "for", "while", "return", "sizeof", "int", "char", "struct",
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
  FILE* fp = fopen(filename, "r");
  if (!fp) {
    error("cannot open %s: %s", filename, strerror(errno));
  }

  if (fseek(fp, 0, SEEK_END) != 0) {
    error("cannot seek %s: %s", filename, strerror(errno));
  }
  size_t size = ftell(fp);
  if (fseek(fp, 0, SEEK_SET) != 0) {
    error("cannot seek %s: %s", filename, strerror(errno));
  }

  user_input = calloc(1, size + 2);
  fread(user_input, size, 1, fp);
  if (size == 0 || user_input[size - 1] != '\n') {
    user_input[size++] = '\n';
  }
  user_input[size] = '\0';

  fclose(fp);
}

void tokenize() {
  read_file();
  char* p = user_input;

  Token head = {};
  Token* cur = &head;
  while (*p) {
    if (isspace(*p)) {
      p++;
      continue;
    }

    if (startswith(p, "//")) {
      while (*p != '\n') {
        p++;
      }
      continue;
    }

    if (startswith(p, "/*")) {
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

    if (startswith(p, "==") || startswith(p, "!=") || startswith(p, "<=") ||
        startswith(p, ">=") || startswith(p, "->")) {
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

    if (is_identable1(*p)) {
      char* start = p;
      do {
        p++;
      } while (is_identable2(*p));
      cur->next = new_token(TK_IDENT, start, p - start);
      cur = cur->next;
      continue;
    }

    if (isdigit(*p)) {
      cur->next = new_token(TK_NUM, p, 0);
      cur->next->val = strtol(p, &p, 10);
      cur = cur->next;
      continue;
    }

    if (*p == '"') {
      char* start = p++;
      for (; *p != '"'; p++) {
        if (*p == '\n' || *p == '\0') {
          error_at(start, "unclosed string literal");
        }
      }
      char* end = p++;

      cur->next = new_token(TK_STR, start + 1, end - start - 1);
      cur = cur->next;
      continue;
    }

    error_at(p, "invalid character");
  }

  cur->next = new_token(TK_EOF, p, 0);
  token = head.next;
}