#include "cc.h"

typedef struct Macro Macro;
typedef struct IfDir IfDir;

struct Macro {
  Macro* next;
  char* name;
  Token* body;
  bool is_like_func;
  Str* params;
};

struct IfDir {
  IfDir* next;
  Token* token;
  bool cond;
  IfDir* elifs;
};

static Macro* macros;
static IfDir* if_dirs;

static Token* preprocess_tokens(Token* tokens);

static bool is_funclike_macro_parens(Token* token) {
  return !token->has_leading_space && equal_to_token(token, "(");
}

static Macro* find_macro(char* c, int len) {
  for (Macro* macro = macros; macro; macro = macro->next) {
    if (!equal_to_n_chars(macro->name, c, len)) {
      continue;
    }

    return macro;
  }

  return NULL;
}

static Macro* find_macro_by_token(Token* token) {
  if (token->kind != TK_IDENT) {
    return NULL;
  }

  Macro* macro = find_macro(token->loc, token->len);
  if (!macro) {
    return NULL;
  }

  return macro->is_like_func == is_funclike_macro_parens(token->next) ? macro : NULL;
}

static void delete_macro(char* name) {
  Macro head = {};
  Macro* cur = &head;
  for (Macro* macro = macros; macro; macro = macro->next) {
    if (equal_to_str(macro->name, name)) {
      continue;
    }
    cur = cur->next = macro;
  }

  macros = head.next;
}

static void add_macro(Macro* macro) {
  delete_macro(macro->name);
  macro->next = macros;
  macros = macro;
}

static Macro* new_macro(char* name, Token* body, bool is_like_func) {
  Macro* macro = calloc(1, sizeof(Macro));
  macro->name = name;
  macro->body = body;
  macro->is_like_func = is_like_func;
  add_macro(macro);
  return macro;
}

static Macro* new_objlike_macro(char* name, Token* body) {
  return new_macro(name, body, false);
}

static Macro* new_funclike_macro(char* name, Token* body, Str* params) {
  Macro* macro = new_macro(name, body, true);
  macro->params = params;
  return macro;
}

static void add_if_dir_to(IfDir** dirs, IfDir* dir) {
  dir->next = *dirs;
  *dirs = dir;
}

static void add_if_dir(IfDir* dir) {
  add_if_dir_to(&if_dirs, dir);
}

static IfDir* new_stray_if_dir(Token* token, bool cond) {
  IfDir* dir = calloc(1, sizeof(IfDir));
  dir->token = token;
  dir->cond = cond;
  return dir;
}

static IfDir* new_if_dir(Token* token, bool cond) {
  IfDir* dir = new_stray_if_dir(token, cond);
  add_if_dir(dir);
  return dir;
}

static Token* skip_extra_tokens(Token* token) {
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
    cur = cur->next = copy_token(token);
  }
  cur->next = latter;

  return head.next;
}

static Token* expect_macro_ident_token(Token** tokens) {
  if ((*tokens)->kind != TK_IDENT) {
    error_token(*tokens, "macro name must be an identifier");
  }
  Token* ident = *tokens;
  *tokens = (*tokens)->next;

  return ident;
}

static Token* inline_tokens(Token** tokens) {
  Token head = {};
  Token* cur = &head;
  for (; *tokens && !(*tokens)->is_bol; *tokens = (*tokens)->next) {
    cur = cur->next = copy_token(*tokens);
  }
  cur->next = new_eof_token_in(cur->file);

  return head.next;
}

static void proceed_token(Token** dst, Token** src) {
  *dst = (*dst)->next = copy_token(*src);
  *src = (*src)->next;
}

static void hand_over_tokens(Token** dst, Token* src) {
  Token* cur = *dst;
  for (Token* token = src; token; token = token->next) {
    cur = cur->next = copy_token(token);
  }
  *dst = cur;
}

static Token* replace_ident_tokens(Token* tokens, char* name, Token* subtokens) {
  Token head = {};
  Token* cur = &head;
  for (Token* token = tokens; token; token = token->next) {
    if (token->kind != TK_IDENT || !equal_to_n_chars(name, token->loc, token->len)) {
      cur = cur->next = copy_token(token);
      continue;
    }

    hand_over_tokens(&cur, subtokens);
  }

  return head.next;
}

static bool can_expand_macro(Token* token) {
  if (token->kind != TK_IDENT) {
    return false;
  }
  if (contain_str(token->hideset, token->loc, token->len)) {
    return false;
  }

  return find_macro_by_token(token) != NULL;
}

static Token* inherit_hideset(Token* dst, Str* hideset) {
  Token head = {};
  Token* cur = &head;
  for (Token* token = dst; token; token = token->next) {
    cur = cur->next = copy_token(token);
    cur->hideset = append_strs(hideset, cur->hideset);
  }

  return head.next;
}

static Token* add_hideset(Token* tokens, Str* hideset) {
  Token head = {};
  Token* cur = &head;
  for (Token* token = tokens; token; token = token->next) {
    cur = cur->next = copy_token(token);
    add_str(&cur->hideset, hideset);
  }

  return head.next;
}

static bool is_funccall(Token* token) {
  return token->kind == TK_IDENT && equal_to_token(token->next, "(");
}

static Token* funclike_macro_funccall_arg(Token** tokens) {
  Token head = {};
  Token* cur = &head;

  cur = cur->next = copy_token(expect_ident_token(tokens));
  cur = cur->next = copy_token(expect_token(tokens, "("));

  while (!equal_to_token(*tokens, ")")) {
    if (is_funccall(*tokens)) {
      hand_over_tokens(&cur, funclike_macro_funccall_arg(tokens));
      continue;
    }

    proceed_token(&cur, tokens);
  }
  proceed_token(&cur, tokens);

  return head.next;
}

static Token* funclike_macro_parenthesized_arg(Token** tokens) {
  Token head = {};
  Token* cur = &head;

  cur = cur->next = copy_token(expect_token(tokens, "("));

  while (!equal_to_token(*tokens, ")")) {
    if (equal_to_token(*tokens, "(")) {
      hand_over_tokens(&cur, funclike_macro_parenthesized_arg(tokens));
      continue;
    }

    proceed_token(&cur, tokens);
  }
  proceed_token(&cur, tokens);

  return head.next;
}

static Token* funclike_macro_arg(Token** tokens) {
  Token head = {};
  Token* cur = &head;
  while (!equal_to_token(*tokens, ",") && !equal_to_token(*tokens, ")")) {
    if (is_funccall(*tokens)) {
      hand_over_tokens(&cur, funclike_macro_funccall_arg(tokens));
      continue;
    }

    if (equal_to_token(*tokens, "(")) {
      hand_over_tokens(&cur, funclike_macro_parenthesized_arg(tokens));
      continue;
    }

    proceed_token(&cur, tokens);
  }

  return head.next;
}

static Token* funclike_macro_body(Macro* macro, Token** tokens) {
  Token* body = macro->body;
  Str* param = macro->params;
  bool is_first = true;
  while (!equal_to_token(*tokens, ")")) {
    if (!is_first) {
      expect_token(tokens, ",");
    }
    is_first = false;

    Token* arg = funclike_macro_arg(tokens);
    arg = preprocess_tokens(arg);

    body = replace_ident_tokens(body, param->data, arg);

    param = param->next;
  }

  return body;
}

static Token* expand_macro(Token* token) {
  Token* ident = token;
  Macro* macro = find_macro_by_token(token);
  if (!macro) {
    error_token(token, "undefined macro");
  }
  token = token->next;

  Token* body = macro->body;
  Str* hideset = ident->hideset;
  if (macro->is_like_func) {
    expect_token(&token, "(");
    body = funclike_macro_body(macro, &token);
    hideset = intersect_strs(hideset, expect_token(&token, ")")->hideset);
  }

  body = inherit_hideset(body, hideset);
  body = add_hideset(body, new_str(macro->name));

  return append(body, token);
}

static Str* funclike_macro_params(Token** tokens) {
  expect_token(tokens, "(");

  Str head = {};
  Str* cur = &head;
  while (!consume_token(tokens, ")")) {
    if (cur != &head) {
      expect_token(tokens, ",");
    }

    Token* ident = copy_token(expect_ident_token(tokens));
    char* name = strndup(ident->loc, ident->len);

    cur = cur->next = new_str(name);
  }

  return head.next;
}

static Token* define_dir(Token* token) {
  expect_dir(&token, "define");

  Token* ident = expect_macro_ident_token(&token);
  char* name = strndup(ident->loc, ident->len);

  bool is_like_func = is_funclike_macro_parens(token);
  if (is_like_func) {
    Str* params = funclike_macro_params(&token);
    new_funclike_macro(name, inline_tokens(&token), params);
  } else {
    new_objlike_macro(name, inline_tokens(&token));
  }

  return token;
}

static Token* undef_dir(Token* token) {
  expect_dir(&token, "undef");

  Token* ident = expect_macro_ident_token(&token);
  char* name = strndup(ident->loc, ident->len);

  delete_macro(name);

  return token;
}

static Token* include_dir(Token* token) {
  expect_dir(&token, "include");

  if (token->kind != TK_STR) {
    error_token(token, "expected a filename");
  }
  char* fname = !start_with(token->str_val, "/") ? format("%s/%s", dirname(strdup(token->file->name)), token->str_val)
                                                 : token->str_val;
  token = token->next;

  token = skip_extra_tokens(token);

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

    token = token->next;
  }
  return token;
}

static Token* skip_if_block(Token* token) {
  while (!is_dir(token, "elif") && !is_dir(token, "else") && !is_dir(token, "endif")) {
    if (is_dir(token, "if") || is_dir(token, "ifdef") || is_dir(token, "ifndef")) {
      token = skip_to_endif_dir(token->next);
      expect_dir(&token, "endif");
      continue;
    }

    token = token->next;
  }
  return token;
}

static bool have_expanded_if_block(IfDir* dir) {
  if (dir->cond) {
    return true;
  }

  for (IfDir* elif = dir->elifs; elif; elif = elif->next) {
    if (elif->cond) {
      return true;
    }
  }

  return false;
}

static bool cond(Token** tokens) {
  Token* cond = inline_tokens(tokens);
  cond = preprocess_tokens(cond);
  return const_expr(&cond) != 0;
}

static Token* ifdef_dir(Token* token) {
  Token* start = token;
  expect_dir(&token, "ifdef");

  Macro* macro = find_macro(token->loc, token->len);
  token = token->next;
  IfDir* dir = new_if_dir(start, macro != NULL);
  if (!dir->cond) {
    token = skip_if_block(token);
  }

  return token;
}

static Token* ifndef_dir(Token* token) {
  Token* start = token;
  expect_dir(&token, "ifndef");

  Macro* macro = find_macro(token->loc, token->len);
  token = token->next;
  IfDir* dir = new_if_dir(start, macro == NULL);
  if (!dir->cond) {
    token = skip_if_block(token);
  }

  return token;
}

static Token* if_dir(Token* token) {
  Token* start = token;
  expect_dir(&token, "if");

  IfDir* dir = new_if_dir(start, cond(&token));
  if (!dir->cond) {
    token = skip_if_block(token);
  }

  return token;
}

static Token* elif_dir(Token* token) {
  Token* start = token;
  expect_dir(&token, "elif");
  if (!if_dirs) {
    error_token(start, "stray #elif");
  }

  IfDir* dir = new_stray_if_dir(start, cond(&token));
  if (have_expanded_if_block(if_dirs) || !dir->cond) {
    token = skip_if_block(token);
  }

  add_if_dir_to(&if_dirs->elifs, dir);

  return token;
}

static Token* else_dir(Token* token) {
  Token* start = token;
  expect_dir(&token, "else");
  if (!if_dirs) {
    error_token(start, "stray #else");
  }

  token = skip_extra_tokens(token);

  if (have_expanded_if_block(if_dirs)) {
    token = skip_to_endif_dir(token->next);
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

  return skip_extra_tokens(token);
}

static bool is_keyword(char* c, int len) {
  static char* keywords[] = {
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
  static int klen = sizeof(keywords) / sizeof(char*);

  for (int i = 0; i < klen; i++) {
    if (!can_be_keyword(c, len)) {
      continue;
    }
    if (equal_to_n_chars(keywords[i], c, len)) {
      return true;
    }
  }

  return false;
}

static Token* convert_keywords(Token* tokens) {
  Token head = {};
  Token* cur = &head;
  for (Token* token = tokens; token; token = token->next) {
    cur = cur->next = token;

    if (token->kind != TK_IDENT) {
      continue;
    }
    if (!is_keyword(token->loc, token->len)) {
      continue;
    }

    cur->kind = TK_RESERVED;
  }

  return head.next;
}

static Token* preprocess_tokens(Token* tokens) {
  Token head = {};
  Token* cur = &head;
  for (Token* token = tokens; token; token = token->next) {
    if (can_expand_macro(token)) {
      token->next = expand_macro(token);
      continue;
    }

    if (!equal_to_token(token, "#")) {
      cur = cur->next = token;
      continue;
    }

    if (is_dir(token, "define")) {
      token->next = define_dir(token);
      continue;
    }
    if (is_dir(token, "undef")) {
      token->next = undef_dir(token);
      continue;
    }

    if (is_dir(token, "include")) {
      token->next = include_dir(token);
    }

    if (is_dir(token, "ifdef")) {
      token->next = ifdef_dir(token);
      continue;
    }
    if (is_dir(token, "ifndef")) {
      token->next = ifndef_dir(token);
      continue;
    }

    if (is_dir(token, "if")) {
      token->next = if_dir(token);
      continue;
    }
    if (is_dir(token, "elif")) {
      token->next = elif_dir(token);
      continue;
    }
    if (is_dir(token, "else")) {
      token->next = else_dir(token);
      continue;
    }
    if (is_dir(token, "endif")) {
      token->next = endif_dir(token);
      continue;
    }

    // null directive
    if (token->next->is_bol) {
      continue;
    }

    error_token(token, "invalid preprocessor directive");
  }

  return head.next;
}

Token* preprocess(Token* tokens) {
  tokens = preprocess_tokens(tokens);
  if (if_dirs) {
    error_token(if_dirs->token, "unterminated if directive");
  }

  return convert_keywords(tokens);
}