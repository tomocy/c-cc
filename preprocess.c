#include "cc.h"

Token* append(Token* former, Token* latter) {
  Token head = {};
  Token* cur = &head;
  for (Token* token = former; token && token->kind != TK_EOF; token = token->next) {
    cur = cur->next = token;
  }
  for (Token* token = latter; token; token = token->next) {
    cur = cur->next = token;
  }

  return head.next;
}

static Token* include(Token* token) {
  expect_token(&token, "#");
  expect_token(&token, "include");

  if (token->kind != TK_STR) {
    error_token(token, "expected a filename");
  }
  char* fname = format("%s/%s", dirname(strdup(token->file->name)), token->str_val);
  token = token->next;

  Token* included = tokenize(fname);
  return append(included, token);
}

Token* preprocess(Token* tokens) {
  Token head = {};
  Token* cur = &head;
  for (Token* token = tokens; token; token = token->next) {
    if (!equal_to_token(token, "#")) {
      cur = cur->next = token;
      continue;
    }

    if (token->is_bol) {
      if (equal_to_token(token->next, "include")) {
        token->next = include(token);
        continue;
      }

      // null directive
      if (token->next->is_bol) {
        continue;
      }
    }

    error_token(token, "invalid preprocessor directive");
  }

  return head.next;
}