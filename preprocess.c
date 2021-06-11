#include "cc.h"

typedef struct IfDir IfDir;

struct IfDir {
  IfDir* next;
  Token* token;
};

static IfDir* if_dirs;

static void add_if_dir(IfDir* dir) {
  dir->next = if_dirs;
  if_dirs = dir;
}

static IfDir* new_if_dir(Token* token) {
  IfDir* dir = calloc(1, sizeof(IfDir));
  dir->token = token;
  add_if_dir(dir);
  return dir;
}

static Token* skip_to_bol(Token* token) {
  if (token->is_bol) {
    return token;
  }

  warn_token(token, "extra token");
  while (!token->is_bol) {
    token = token->next;
  }
  return token;
}

static bool is_hash(Token* token) {
  return equal_to_token(token, "#") && token->is_bol;
}

static bool is_dir(Token* token, char* dir) {
  return is_hash(token) && equal_to_token(token->next, dir);
}

static void expect_dir(Token** tokens, char* dir) {
  if (!is_dir(*tokens, dir)) {
    error_token(*tokens, "expected %s directive", dir);
  }
  expect_token(tokens, "#");
  expect_token(tokens, dir);
}

static Token* append(Token* former, Token* latter) {
  Token head = {};
  Token* cur = &head;
  for (Token* token = former; token && token->kind != TK_EOF; token = token->next) {
    cur = cur->next = token;
  }
  cur->next = latter;

  return head.next;
}

static Token* include_dir(Token* token) {
  expect_dir(&token, "include");

  if (token->kind != TK_STR) {
    error_token(token, "expected a filename");
  }
  char* fname = !start_with(token->str_val, "/") ? format("%s/%s", dirname(strdup(token->file->name)), token->str_val)
                                                 : token->str_val;
  token = token->next;

  token = skip_to_bol(token);

  Token* included = tokenize(fname);
  return append(included, token);
}

static Token* skip_to_endif_dir(Token* token) {
  while (!is_dir(token, "endif")) {
    if (is_dir(token, "if")) {
      token = skip_to_endif_dir(token->next);
      expect_dir(&token, "endif");
      continue;
    }
    if (is_dir(token, "endif")) {
      break;
    }

    token = token->next;
  }
  return token;
}

static Token* if_dir(Token* token) {
  Token* start = token;
  expect_dir(&token, "if");
  new_if_dir(start);

  int64_t cond = const_expr(&token);
  if (!cond) {
    token = skip_to_endif_dir(token);
  }

  return token;
}

static Token* endif_dir(Token* token) {
  Token* start = token;
  expect_dir(&token, "endif");
  if (!if_dirs) {
    error_token(start, "stray #endif");
  }
  if_dirs = if_dirs->next;

  return skip_to_bol(token);
}

Token* preprocess(Token* tokens) {
  Token head = {};
  Token* cur = &head;
  for (Token* token = tokens; token; token = token->next) {
    if (!equal_to_token(token, "#")) {
      cur = cur->next = token;
      continue;
    }

    if (is_dir(token, "include")) {
      token->next = include_dir(token);
    }

    if (is_dir(token, "if")) {
      token->next = if_dir(token);
    }
    if (is_dir(token, "endif")) {
      token->next = endif_dir(token);
    }

    // null directive
    if (token->next->is_bol) {
      continue;
    }

    error_token(token, "invalid preprocessor directive");
  }

  if (if_dirs) {
    error_token(if_dirs->token, "unterminated if directive");
  }

  return head.next;
}