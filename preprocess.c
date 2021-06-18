#include "cc.h"

typedef struct Macro Macro;
typedef struct FunclikeMacroArg FunclikeMacroArg;
typedef struct IfDir IfDir;

struct Macro {
  Macro* next;
  char* name;
  Token* body;
  bool is_like_func;
  Str* params;
};

struct FunclikeMacroArg {
  FunclikeMacroArg* next;
  char* param;
  Token* token;
};

struct IfDir {
  IfDir* next;
  Token* token;
  bool cond;
  IfDir* elifs;
};

static Macro* macros;
static IfDir* if_dirs;

static Token* process(Token* tokens);

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

  bool is_like_func = token->next ? is_funclike_macro_parens(token->next) : false;
  return macro->is_like_func == is_like_func ? macro : NULL;
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

static Macro* create_macro(char* name, Token* body, bool is_like_func) {
  Macro* macro = calloc(1, sizeof(Macro));
  macro->name = name;
  macro->body = body;
  macro->is_like_func = is_like_func;
  add_macro(macro);
  return macro;
}

static Macro* create_objlike_macro(char* name, Token* body) {
  return create_macro(name, body, false);
}

static Macro* create_funclike_macro(char* name, Str* params, Token* body) {
  Macro* macro = create_macro(name, body, true);
  macro->params = params;
  return macro;
}

static FunclikeMacroArg* new_funclike_macro_arg(char* param, Token* token) {
  FunclikeMacroArg* arg = calloc(1, sizeof(FunclikeMacroArg));
  arg->param = param;
  arg->token = token;
  return arg;
}

static FunclikeMacroArg* find_funclike_macro_arg_from(FunclikeMacroArg* args, char* c, int len) {
  for (FunclikeMacroArg* arg = args; arg; arg = arg->next) {
    if (!equal_to_n_chars(arg->param, c, len)) {
      continue;
    }

    return arg;
  }

  return NULL;
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
  if (!token) {
    return NULL;
  }

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

static Token* tokenize_as_if(File* file, char* contents) {
  return append(tokenize_in(copy_file_with_contents(file, contents)), NULL);
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

static char* restore_contents(Token* tokens) {
  int len = 1;  // the 1 for EOF
  for (Token* token = tokens; token; token = token->next) {
    if (token != tokens && token->has_leading_space) {
      len++;
    }
    len += token->len;
  }

  // consecutive spaces are recognized as one space
  char* contents = calloc(len, sizeof(char));
  char* c = contents;
  for (Token* token = tokens; token; token = token->next) {
    if (token != tokens && token->has_leading_space) {
      *c++ = ' ';
    }
    strncpy(c, token->loc, token->len);
    c += token->len;
  }
  *c++ = '\0';

  return contents;
}

// NOLINTNEXTLINE
static char* quote_str(char* s) {
  int len = 3;  // the min len, for ""\0
  for (int i = 0; s[i]; i++) {
    // Those characters should be escaped
    if (s[i] == '\\' || s[i] == '"') {
      len++;
    }
    len++;
  }

  char* val = calloc(len, sizeof(char));
  char* c = val;
  *c++ = '"';
  for (int i = 0; s[i]; i++) {
    if (s[i] == '\\' || s[i] == '"') {
      *c++ = '\\';
    }
    *c++ = s[i];
  }
  *c++ = '"';
  *c++ = '\0';

  return val;
}

static Token* stringize_tokens(Token* tokens) {
  // it is better to set some values in those tokens to their locations so that
  // the error reporting can indicate the locations
  // , so tokenize those tokens as if they are in the file of the tokens which are replaced
  char* quoted = quote_str(restore_contents(tokens));
  return tokenize_as_if(tokens->file, quoted);
}

static Token* concat_tokens(Token* lhs, Token* rhs) {
  File* file = lhs->file ? lhs->file : rhs->file;
  char* contents = format("%.*s%.*s", lhs->len, lhs->loc, rhs->len, rhs->loc);
  return tokenize_as_if(file, contents);
}

static Token* inherit_token_space(Token* dst, Token* src) {
  Token* next = dst->next;
  Token* token = copy_token(dst);
  token->next = next;
  token->is_bol = src->is_bol;
  token->has_leading_space = src->has_leading_space;
  return token;
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
    add_str(&cur->hideset, copy_str(hideset));
  }

  return head.next;
}

static Token* funclike_macro_arg(Token** tokens) {
  Token head = {};
  Token* cur = &head;
  int depth = 0;
  while (depth > 0 || (!equal_to_token(*tokens, ",") && !equal_to_token(*tokens, ")"))) {
    if (equal_to_token(*tokens, "(")) {
      depth++;
    }
    if (equal_to_token(*tokens, ")")) {
      depth--;
    }

    proceed_token(&cur, tokens);
  }

  return head.next;
}

static FunclikeMacroArg* funclike_macro_args(Str* params, Token** tokens) {
  Token* start = *tokens;

  FunclikeMacroArg head = {};
  FunclikeMacroArg* cur = &head;
  Str* param = params;
  while (!equal_to_token(*tokens, ")")) {
    if (cur != &head) {
      expect_token(tokens, ",");
    }
    if (!param) {
      error_token(*tokens, "too many arguments");
    }

    Token* token = funclike_macro_arg(tokens);
    FunclikeMacroArg* arg = new_funclike_macro_arg(param->data, token);

    cur = cur->next = arg;
    param = param->next;
  }
  if (param) {
    error_token(start, "too few arguments");
  }

  return head.next;
}

static Token* concat_funclike_macro_body(Token** tokens, FunclikeMacroArg* args) {
  Token head = {};
  Token* cur = &head;

  FunclikeMacroArg* arg = find_funclike_macro_arg_from(args, (*tokens)->loc, (*tokens)->len);
  if (arg) {
    hand_over_tokens(&cur, arg->token);
  } else {
    cur = cur->next = copy_token(*tokens);
  }

  while (equal_to_token((*tokens)->next, "##")) {
    if (!(*tokens)->next->next) {
      error_token((*tokens)->next, "'##' cannot appear at the end of macro expansion");
    }

    *tokens = (*tokens)->next->next;

    arg = find_funclike_macro_arg_from(args, (*tokens)->loc, (*tokens)->len);
    if (arg) {
      if (arg->token) {
        if (cur != &head) {
          *cur = *concat_tokens(cur, arg->token);
        } else {
          cur = cur->next = copy_token(arg->token);
        }
        hand_over_tokens(&cur, arg->token->next);
      }
    } else {
      *cur = *concat_tokens(cur, *tokens);
    }
  }

  return head.next;
}

static Token* replace_funclike_macro_body(Token* body, FunclikeMacroArg* args) {
  Token head = {};
  Token* cur = &head;
  for (Token* token = body; token; token = token->next) {
    if (token == body && equal_to_token(token, "##")) {
      error_token(token, "'##' cannot appear at the start of macro expansion");
    }

    if (token->next && equal_to_token(token->next, "##")) {
      hand_over_tokens(&cur, concat_funclike_macro_body(&token, args));
      continue;
    }

    if (equal_to_token(token, "#") && token->next) {
      Token* start = token;

      expect_token(&token, "#");
      FunclikeMacroArg* arg = find_funclike_macro_arg_from(args, token->loc, token->len);
      if (!arg) {
        error_token(start, "'#' is not followed by a macro parameter");
      }

      hand_over_tokens(&cur, stringize_tokens(arg->token));
      continue;
    }

    FunclikeMacroArg* arg = find_funclike_macro_arg_from(args, token->loc, token->len);
    if (arg) {
      Token* processed = process(arg->token);
      if (processed) {
        processed = inherit_token_space(processed, token);
        hand_over_tokens(&cur, processed);
      }
      continue;
    }

    cur = cur->next = copy_token(token);
  }

  return head.next;
}

static Token* funclike_macro_body(Macro* macro, Token** tokens) {
  FunclikeMacroArg* args = funclike_macro_args(macro->params, tokens);
  return replace_funclike_macro_body(macro->body, args);
}

static Token* expand_macro(Token* token) {
  Token* ident = token;
  Macro* macro = find_macro_by_token(token);
  if (!macro) {
    error_token(token, "undefined macro");
  }
  token = token->next;

  Token* body = inherit_token_space(macro->body, ident);
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

static Token* macro_body(Token** tokens) {
  Token* body = inline_tokens(tokens);
  if (body) {
    body->is_bol = false;
    body->has_leading_space = false;
  }

  return body;
}

static Token* define_dir(Token* token) {
  expect_dir(&token, "define");

  Token* ident = expect_macro_ident_token(&token);
  char* name = strndup(ident->loc, ident->len);

  bool is_like_func = is_funclike_macro_parens(token);
  if (is_like_func) {
    Str* params = funclike_macro_params(&token);
    create_funclike_macro(name, params, macro_body(&token));
  } else {
    create_objlike_macro(name, macro_body(&token));
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

static char* compensate_include_filename(char* fname) {
  if (start_with(fname, "/")) {
    return fname;
  }

  for (Str* path = include_paths; path; path = path->next) {
    char* compensated = format("%s/%s", path->data, fname);
    if (have_file(compensated)) {
      return compensated;
    }
  }
  return fname;
}

static char* include_filename(Token** tokens) {
  // #include "..."
  if ((*tokens)->kind == TK_STR) {
    // str_val cannot be used as a filename as it is escaped
    char* raw_fname = strndup((*tokens)->loc + 1, (*tokens)->len - 2);
    char* fname = raw_fname;
    if (!start_with(fname, "/")) {
      fname = format("%s/%s", dir((*tokens)->file->name), fname);
    }
    if (!have_file(fname)) {
      fname = compensate_include_filename(raw_fname);
    }
    *tokens = (*tokens)->next;

    *tokens = skip_extra_tokens(*tokens);

    return fname;
  }

  // #include <...>
  if (consume_token(&*tokens, "<")) {
    Token head = {};
    Token* cur = &head;
    while (!consume_token(&*tokens, ">")) {
      if ((*tokens)->is_bol) {
        error_token(*tokens, "expected '>'");
      }

      cur = cur->next = copy_token(*tokens);
      *tokens = (*tokens)->next;
    }
    *tokens = skip_extra_tokens(*tokens);

    return compensate_include_filename(restore_contents(head.next));
  }

  // #include MACRO
  if ((*tokens)->kind == TK_IDENT) {
    Token* next = (*tokens)->next;
    Token* processed = append(process(copy_token(*tokens)), next);
    char* fname = include_filename(&processed);
    *tokens = processed;
    return fname;
  }

  error_token(*tokens, "expected a filename");
  return NULL;
}

static Token* include_dir(Token* token) {
  expect_dir(&token, "include");
  char* fname = include_filename(&token);
  return append(tokenize(fname), token);
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

static Token* if_dir_defined_cond(Token** tokens) {
  Token* start = *tokens;

  expect_token(tokens, "defined");
  bool with_parens = consume_token(tokens, "(");

  Token* ident = expect_macro_ident_token(tokens);
  Macro* macro = find_macro(ident->loc, ident->len);

  if (with_parens) {
    expect_token(tokens, ")");
  }

  return tokenize_as_if(start->file, format("%d", macro != NULL));
}

static bool if_dir_cond(Token** tokens) {
  Token head = {};
  Token* cur = &head;
  for (Token* token = inline_tokens(tokens); token;) {
    if (equal_to_token(token, "defined")) {
      hand_over_tokens(&cur, if_dir_defined_cond(&token));
      continue;
    }

    cur = cur->next = token;
    token = token->next;
  }

  Token* cond = head.next;
  cond = append(cond, new_eof_token_in(cond->file));
  cond = process(cond);

  for (Token* token = cond; token; token = token->next) {
    if (token->kind != TK_IDENT) {
      continue;
    }

    Token* next = token->next;
    *token = *tokenize_as_if(token->file, format("%d", 0));
    token->next = next;
  }

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

  IfDir* dir = new_if_dir(start, if_dir_cond(&token));
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

  IfDir* dir = new_stray_if_dir(start, if_dir_cond(&token));
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

static Token* process(Token* tokens) {
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
      continue;
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
  tokens = process(tokens);
  if (if_dirs) {
    error_token(if_dirs->token, "unterminated if directive");
  }

  return convert_keywords(tokens);
}