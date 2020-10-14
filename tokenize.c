#include "cc.h"

char* user_input;

Token* token;

void error(char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

void error_at(char* loc, char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  int pos = loc - user_input;
  fprintf(stderr, "%s\n", user_input);
  fprintf(stderr, "%*s", pos, "");
  fprintf(stderr, "^ ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

bool equal(Token* tok, char* op) {
  return tok->kind == TK_RESERVED && memcmp(tok->str, op, tok->len) == 0 &&
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
    error_at(token->str, "expected '%s'", op);
  }
  token = token->next;
}

int expect_num() {
  if (token->kind != TK_NUM) {
    error_at(token->str, "expected a number");
  }

  int val = token->val;
  token = token->next;
  return val;
}

bool at_eof() { return token->kind == TK_EOF; }

Token* new_token(TokenKind kind, char* str, int len) {
  Token* tok = calloc(1, sizeof(Token));
  tok->kind = kind;
  tok->str = str;
  tok->len = len;
  return tok;
}

bool startswith(char* p, char* q) { return memcmp(p, q, strlen(q)) == 0; }

bool is_alnum(char c) {
  return ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z') ||
         ('0' <= c && c <= '9');
}

bool equal_str(char* p, char* keyword) {
  return startswith(p, keyword) && !is_alnum(p[strlen(keyword)]);
}

bool consume_keyword(Token** tok, char** p) {
  static char* ks[] = {"if", "else", "for", "while", "return"};
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

void tokenize() {
  char* p = user_input;

  Token head = {};
  Token* cur = &head;
  while (*p) {
    if (isspace(*p)) {
      p++;
      continue;
    }

    if (consume_keyword(&cur->next, &p)) {
      cur = cur->next;
      continue;
    }

    if (startswith(p, "==") || startswith(p, "!=") || startswith(p, "<=") ||
        startswith(p, ">=")) {
      cur->next = new_token(TK_RESERVED, p, 2);
      cur = cur->next;
      p += 2;
      continue;
    }

    if (strchr("+-*/()<>=;{},&", *p)) {
      cur->next = new_token(TK_RESERVED, p++, 1);
      cur = cur->next;
      continue;
    }

    if (isdigit(*p)) {
      cur->next = new_token(TK_NUM, p, 0);
      cur->next->val = strtol(p, &p, 10);
      cur = cur->next;
      continue;
    }

    if ('a' <= *p && *p <= 'z') {
      char* start = p;
      do {
        p++;
      } while ('a' <= *p && *p <= 'z');
      cur->next = new_token(TK_IDENT, start, p - start);
      cur = cur->next;
      continue;
    }

    error_at(p, "invalid character");
  }

  cur->next = new_token(TK_EOF, p, 0);
  token = head.next;
}