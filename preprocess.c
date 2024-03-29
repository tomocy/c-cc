#include "cc.h"

typedef struct Macro Macro;
typedef Token* MacroBodyGenerator(Token*);
typedef struct FunclikeMacroArg FunclikeMacroArg;
typedef struct IfDir IfDir;

struct Macro {
  char* name;
  Token* body;
  MacroBodyGenerator* gen;
  bool is_like_func;
  Str* params;
  char* va_param;
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

static char* last_included_path;

static Map macros;
static IfDir* if_dirs;
static Map include_guards;
static Map pragma_once;

static Token* process(Token* tokens);

static bool is_funclike_macro_define_parens(Token* token) {
  return !token->has_leading_space && equal_to_token(token, "(");
}

static Macro* find_macro(char* s) {
  return get_from_map(&macros, s);
}

static Macro* find_macro_by_token(Token* token) {
  if (token->kind != TK_IDENT) {
    return NULL;
  }

  return find_macro(strndup(token->loc, token->len));
}

void undefine_macro(char* name) {
  delete_from_map(&macros, name);
}

static void add_macro(Macro* macro) {
  put_to_map(&macros, macro->name, macro);
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

static Macro* create_funclike_macro(char* name, Str* params, char* va_param, Token* body) {
  Macro* macro = create_macro(name, body, true);
  macro->params = params;
  macro->va_param = va_param;
  return macro;
}

static Macro* create_macro_with_generator(char* name, MacroBodyGenerator* gen) {
  Macro* macro = calloc(1, sizeof(Macro));
  macro->name = name;
  macro->gen = gen;
  add_macro(macro);
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

static IfDir* create_if_dir(Token* token, bool cond) {
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
  expect_tokens(tokens, "#", dir, NULL);
}

static Token* tokenize_as_if(File* file, int line, char* contents) {
  Token* tokens = append_tokens(tokenize_in(copy_file_with_contents(file, contents)), NULL);
  for (Token* token = tokens; token; token = token->next) {
    token->line = line + token->line - 1;
  }

  return tokens;
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
  // the error reporting can indicate the locations,
  // so tokenize those tokens as if they are in the file of the tokens which are replaced
  char* quoted = quote_str(restore_contents(tokens));
  return tokenize_as_if(tokens->file, tokens->line, quoted);
}

static Token* inherit_token_space(Token* token, Token* src) {
  if (!token) {
    return NULL;
  }

  Token* next = token->next;
  token = copy_token(token);
  token->next = next;
  token->is_bol = src->is_bol;
  token->has_leading_space = src->has_leading_space;
  return token;
}

static Token* concat_tokens(Token* lhs, Token* rhs) {
  File* file = lhs->file ? lhs->file : rhs->file;
  char* contents = format("%.*s%.*s", lhs->len, lhs->loc, rhs->len, rhs->loc);
  return inherit_token_space(tokenize_as_if(file, lhs->line, contents), lhs);
}

static Token* inherit_original_token(Token* token, Token* original) {
  if (!token) {
    return NULL;
  }

  original = copy_token(original);
  token = inherit_token_space(token, original);

  Token head = {};
  Token* cur = &head;
  for (Token* t = token; t; t = t->next) {
    cur = cur->next = copy_token(t);
    cur->original = original;
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

  Macro* macro = find_macro_by_token(token);
  if (!macro) {
    return false;
  }

  if (!macro->is_like_func) {
    return true;
  }

  // Funclike macro is expanded only when the token sequence is
  // like funccall
  return equal_to_token(token->next, "(");
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

void define_builtin_macro(char* name, char* raw_body) {
  create_objlike_macro(name, tokenize_as_if(new_file(1, "built-in", ""), 1, raw_body));
}

static char* format_date(struct tm* time) {
  // for 3 + 1 (for null termination)
  static char mons[][4] = {
    "Jan",
    "Feb",
    "Mar",
    "Ari",
    "May",
    "Jun",
    "Jul",
    "Aug",
    "Sep",
    "Oct",
    "Nov",
    "Dec",
  };

  return format("\"%s %2d %d\"", mons[time->tm_mon], time->tm_mday, time->tm_year + 1900);
}

static char* format_time(struct tm* time) {
  return format("\"%02d:%02d:%02d\"", time->tm_hour, time->tm_min, time->tm_sec);
}

static Token* gen_filename(Token* token) {
  while (token->original) {
    token = token->original;
  }
  return tokenize_as_if(token->file, token->line, quote_str(token->file->name));
}

static Token* gen_line(Token* token) {
  while (token->original) {
    token = token->original;
  }
  return tokenize_as_if(token->file, token->line, format("%d", token->line));
}

static Token* gen_count(Token* token) {
  static int n = 0;
  return tokenize_as_if(token->file, token->line, format("%d", n++));
}

static Token* gen_input_filename(Token* token) {
  return tokenize_as_if(token->file, token->line, format("\"%s\"", input_filename));
}

static Token* gen_file_last_modified_time(Token* token) {
  if (!have_file(token->file->name)) {
    return tokenize_as_if(token->file, token->line, "\"??? ??? ?? ??:??:?? ????\"");
  }

  struct stat st = {};
  stat(token->file->name, &st);
  char* time = calloc(25, sizeof(char));
  ctime_r(&st.st_mtime, time);
  time[24] = '\0';
  return tokenize_as_if(token->file, token->line, format("\"%s\"", time));
}

void define_builtin_macros(File* file) {
  define_builtin_macro("_LP64", "1");
  define_builtin_macro("__C99_MACRO_WITH_VA_ARGS", "1");
  define_builtin_macro("__ELF__", "1");
  define_builtin_macro("__LP64__", "1");
  define_builtin_macro("__SIZEOF_DOUBLE__", "8");
  define_builtin_macro("__SIZEOF_FLOAT__", "4");
  define_builtin_macro("__SIZEOF_INT__", "4");
  define_builtin_macro("__SIZEOF_LONG_DOUBLE__", "8");
  define_builtin_macro("__SIZEOF_LONG_LONG__", "8");
  define_builtin_macro("__SIZEOF_LONG__", "8");
  define_builtin_macro("__SIZEOF_POINTER__", "8");
  define_builtin_macro("__SIZEOF_PTRDIFF_T__", "8");
  define_builtin_macro("__SIZEOF_SHORT__", "2");
  define_builtin_macro("__SIZEOF_SIZE_T__", "8");
  define_builtin_macro("__SIZE_TYPE__", "unsigned long");
  define_builtin_macro("__STDC_HOSTED__", "1");
  define_builtin_macro("__STDC_NO_COMPLEX__", "1");
  define_builtin_macro("__STDC_UTF_16__", "1");
  define_builtin_macro("__STDC_UTF_32__", "1");
  define_builtin_macro("__STDC_VERSION__", "201112L");
  define_builtin_macro("__STDC__", "1");
  define_builtin_macro("__USER_LABEL_PREFIX__", "");
  define_builtin_macro("__alignof__", "_Alignof");
  define_builtin_macro("__amd64", "1");
  define_builtin_macro("__amd64__", "1");
  define_builtin_macro("__chibicc__", "1");
  define_builtin_macro("__const__", "const");
  define_builtin_macro("__gnu_linux__", "1");
  define_builtin_macro("__inline__", "inline");
  define_builtin_macro("__linux", "1");
  define_builtin_macro("__linux__", "1");
  define_builtin_macro("__signed__", "signed");
  define_builtin_macro("__typeof__", "typeof");
  define_builtin_macro("__unix", "1");
  define_builtin_macro("__unix__", "1");
  define_builtin_macro("__volatile__", "volatile");
  define_builtin_macro("__x86_64", "1");
  define_builtin_macro("__x86_64__", "1");
  define_builtin_macro("linux", "1");
  define_builtin_macro("unix", "1");

  time_t timer = time(NULL);
  struct tm* now = localtime(&timer);
  define_builtin_macro("__DATE__", format_date(now));
  define_builtin_macro("__TIME__", format_time(now));

  create_macro_with_generator("__FILE__", gen_filename);
  create_macro_with_generator("__LINE__", gen_line);
  create_macro_with_generator("__COUNTER__", gen_count);

  create_macro_with_generator("__BASE_FILE__", gen_input_filename);
  create_macro_with_generator("__TIMESTAMP__", gen_file_last_modified_time);
}

static bool is_keyword(char* c, int len) {
  static Map map;

  if (map.cap == 0) {
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
      "asm",
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
      "_Thread_local",
      "__thread",
      "_Atomic",
    };
    static int klen = sizeof(keywords) / sizeof(char*);

    for (int i = 0; i < klen; i++) {
      put_to_map(&map, keywords[i], (void*)true);
    }
  }

  return can_be_keyword(c, len) && get_from_map(&map, strndup(c, len)) != NULL;
}

static bool isbdigit(char c) {
  return c == '0' || c == '1';
}

static Token* convert_pp_int(Token* token) {
  char* c = token->loc;
  char* start = c;

  int base = 10;
  if (start_with_insensitive(c, "0x") && isxdigit(c[2])) {
    c += 2;
    base = 16;
  } else if (start_with_insensitive(c, "0b") && isbdigit(c[2])) {
    c += 2;
    base = 2;
  } else if (*c == '0') {
    base = 8;
  }

  int64_t val = strtoul(c, &c, base);

  enum {
    VANILLA = 1 << 0,
    UNSIGNED = 1 << 1,
    LONG = 1 << 2,
  };
  int spec = VANILLA;
  if (start_with_any(c, "LLU", "LLu", "llU", "llu", "ULL", "Ull", "uLL", "ull", NULL)) {
    c += 3;
    spec = UNSIGNED + LONG;
  } else if (start_with_insensitive(c, "LU") || start_with_insensitive(c, "UL")) {
    c += 2;
    spec = UNSIGNED + LONG;
  } else if (start_with(c, "LL") || start_with_insensitive(c, "ll")) {
    c += 2;
    spec = LONG;
  } else if (start_with_insensitive(c, "L")) {
    c++;
    spec = LONG;
  } else if (start_with_insensitive(c, "U")) {
    c++;
    spec = UNSIGNED;
  }

  if (c != start + token->len) {
    return NULL;
  }

  Type* type = NULL;
  if (base == 10) {
    switch (spec) {
      case UNSIGNED + LONG:
        type = new_ulong_type();
        break;
      case UNSIGNED:
        type = val >> 32 ? new_ulong_type() : new_uint_type();
        break;
      case LONG:
        type = new_long_type();
      default:
        type = val >> 31 ? new_long_type() : new_int_type();
    }
  } else {
    switch (spec) {
      case UNSIGNED + LONG:
        type = new_ulong_type();
        break;
      case UNSIGNED:
        type = val >> 32 ? new_ulong_type() : new_uint_type();
        break;
      case LONG:
        type = val >> 63 ? new_ulong_type() : new_long_type();
        break;
      default:
        if (val >> 63) {
          type = new_ulong_type();
        } else if (val >> 32) {
          type = new_long_type();
        } else if (val >> 31) {
          type = new_uint_type();
        } else {
          type = new_int_type();
        }
    }
  }

  Token* converted = new_token_in(TK_NUM, token->file, start, c - start);
  converted->line = token->line;
  converted->type = type;
  converted->int_val = val;
  return inherit_token_space(converted, token);
}

static Token* convert_pp_float(Token* token) {
  char* c = token->loc;
  char* start = c;

  long double val = strtod(start, &c);

  Type* type = NULL;
  if (start_with_insensitive(c, "F")) {
    type = new_float_type();
    c++;
  } else if (start_with_insensitive(c, "L")) {
    type = new_ldouble_type();
    c++;
  } else {
    type = new_double_type();
  }

  if (c != start + token->len) {
    return NULL;
  }

  Token* converted = new_token_in(TK_NUM, token->file, start, c - start);
  converted->line = token->line;
  converted->type = type;
  converted->float_val = val;
  return inherit_token_space(converted, token);
}

static Token* convert_pp_num(Token* token) {
  Token* converted = convert_pp_int(token);
  if (converted) {
    return converted;
  }
  converted = convert_pp_float(token);
  if (converted) {
    return converted;
  }

  error_token(token, "invalid numeric constant");
  return NULL;
}

static Token* convert_tokens(Token* tokens) {
  Token head = {};
  Token* cur = &head;
  for (Token* token = tokens; token; token = token->next) {
    if (token->kind == TK_IDENT && is_keyword(token->loc, token->len)) {
      cur = cur->next = copy_token(token);
      cur->kind = TK_RESERVED;
      continue;
    }

    if (token->kind == TK_PP_NUM) {
      cur = cur->next = convert_pp_num(token);
      continue;
    }

    cur = cur->next = token;
  }

  return head.next;
}

Token* concat_adjecent_strs(Token* tokens) {
  Token head = {};
  Token* cur = &head;

  // Convert regular string literals to wider ones if the regular ones are being concatenated
  // to the wider ones in order to make it easy to concatenate those strings in the latter step.
  for (Token* token = tokens; token; token = token->next) {
    if (token->kind != TK_STR || token->next->kind != TK_STR) {
      cur = cur->next = copy_token(token);
      continue;
    }

    Token* not_str = token->next;
    while (not_str->kind == TK_STR) {
      not_str = not_str->next;
    }

    CharKind concated_kind = token->char_kind;
    int len = 0;
    for (Token* str = token; str != not_str; str = str->next) {
      if (str->char_kind != concated_kind && str->char_kind != CHAR_VANILLA && concated_kind != CHAR_VANILLA) {
        error_token(str, "unsupported concatenation of string literals");
      }
      if (str->char_kind > concated_kind) {
        concated_kind = str->char_kind;
      }

      len += str->type->len - 1;
    }

    for (Token* t = token; t != not_str; t = t->next) {
      if (concated_kind == CHAR_VANILLA || t->char_kind != CHAR_VANILLA) {
        cur = cur->next = copy_token(t);
        continue;
      }

      cur = cur->next = read_str_literal_in(t->file, concated_kind, t->loc, t->loc, t->loc + t->len - 1);
    }

    token->next = not_str;
  }

  for (Token* token = head.next; token; token = token->next) {
    if (token->kind != TK_STR || token->next->kind != TK_STR) {
      continue;
    }

    Token* not_str = token->next;
    while (not_str->kind == TK_STR) {
      not_str = not_str->next;
    }

    int len = 0;
    for (Token* str = token; str != not_str; str = str->next) {
      len += str->type->len - 1;
    }

    char* val = calloc(len + 1, token->type->base->size);
    char* c = val;
    for (Token* str = token; str != not_str; str = str->next) {
      int size = str->type->size - str->type->base->size;
      memcpy(c, str->str_val, size);
      c += size;  // not to proceed by the NULL termination
    }

    token->type = new_array_type(token->type->base, len + 1);
    token->str_val = val;

    token->next = not_str;
  }

  return head.next;
}

static Token* funclike_macro_arg(Token** tokens) {
  Token head = {};
  Token* cur = &head;
  int depth = 0;
  while (depth > 0 || (!equal_to_any_token(*tokens, ",", ")", NULL))) {
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

static FunclikeMacroArg* funclike_macro_args(Macro* macro, Token** tokens) {
  Token* start = *tokens;

  FunclikeMacroArg head = {};
  FunclikeMacroArg* cur = &head;
  Str* param = macro->params;
  while (!equal_to_token(*tokens, ")")) {
    if (cur != &head) {
      expect_token(tokens, ",");
    }

    if (!param) {
      if (!macro->va_param) {
        error_token(*tokens, "too many arguments");
      }
      break;
    }

    Token* token = funclike_macro_arg(tokens);
    cur = cur->next = new_funclike_macro_arg(param->data, token);
    param = param->next;
  }
  if (param) {
    error_token(start, "too few arguments");
  }

  if (macro->va_param) {
    Token token_head = {};
    Token* cur_token = &token_head;
    while (!equal_to_token(*tokens, ")")) {
      if (cur_token != &token_head) {
        cur_token = cur_token->next = expect_token(tokens, ",");
      }

      hand_over_tokens(&cur_token, funclike_macro_arg(tokens));
    }
    cur = cur->next = new_funclike_macro_arg(macro->va_param, token_head.next);
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
          *cur = *inherit_token_space(concat_tokens(cur, arg->token), *tokens);
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

static Token* parenthesized_tokens(Token** tokens) {
  Token head = {};
  Token* cur = &head;
  int depth = 0;
  while (depth > 0 || !equal_to_token(*tokens, ")")) {
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

static Token* replace_funclike_macro_body(Macro* macro, FunclikeMacroArg* args) {
  Token head = {};
  Token* cur = &head;
  for (Token* token = macro->body; token; token = token->next) {
    if (token == macro->body && equal_to_token(token, "##")) {
      error_token(token, "'##' cannot appear at the start of macro expansion");
    }

    // [GNU] the same as __VA_OPT__
    if (macro->va_param && equal_to_tokens(token, ",", "##", macro->va_param, NULL)) {
      Token* semicolon = copy_token(expect_token(&token, ","));

      FunclikeMacroArg* arg = find_funclike_macro_arg_from(args, macro->va_param, strlen(macro->va_param));
      if (arg && arg->token) {
        cur = cur->next = semicolon;
      }
      continue;
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

      hand_over_tokens(&cur, inherit_token_space(stringize_tokens(arg->token), start));
      continue;
    }

    if (equal_to_tokens(token, "__VA_OPT__", "(", NULL)) {
      expect_tokens(&token, "__VA_OPT__", "(", NULL);

      Token* opt = parenthesized_tokens(&token);

      FunclikeMacroArg* arg = find_funclike_macro_arg_from(args, macro->va_param, strlen(macro->va_param));
      if (arg && arg->token) {
        hand_over_tokens(&cur, opt);
      }
      continue;
    }

    FunclikeMacroArg* arg = find_funclike_macro_arg_from(args, token->loc, token->len);
    if (arg) {
      Token* processed = process(copy_tokens(arg->token));
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
  FunclikeMacroArg* args = funclike_macro_args(macro, tokens);
  return replace_funclike_macro_body(macro, args);
}

static Token* expand_macro(Token* token) {
  Token* ident = token;
  Macro* macro = find_macro_by_token(token);
  if (!macro) {
    error_token(token, "undefined macro");
  }
  token = token->next;

  if (macro->gen) {
    Token* body = inherit_original_token(macro->gen(ident), ident);
    body->next = token;
    return body;
  }

  if (macro->is_like_func) {
    expect_token(&token, "(");
    Token* body = inherit_original_token(funclike_macro_body(macro, &token), ident);
    Str* hideset = intersect_strs(ident->hideset, expect_token(&token, ")")->hideset);
    body = inherit_hideset(body, hideset);
    body = add_hideset(body, new_str(macro->name));

    return append_tokens(body, token);
  }

  Token* body = inherit_original_token(macro->body, ident);
  body = inherit_hideset(body, ident->hideset);
  body = add_hideset(body, new_str(macro->name));

  return append_tokens(body, token);
}

static Str* funclike_macro_params(Token** tokens, char** va_param) {
  expect_token(tokens, "(");

  Str head = {};
  Str* cur = &head;
  *va_param = NULL;
  while (!consume_token(tokens, ")")) {
    if (cur != &head) {
      expect_token(tokens, ",");
    }

    if (consume_token(tokens, "...")) {
      *va_param = "__VA_ARGS__";
      expect_token(tokens, ")");
      break;
    }

    Token* ident = copy_token(expect_ident_token(tokens));
    char* name = strndup(ident->loc, ident->len);

    // [GNU] GCC style variadic arguments
    if (consume_token(tokens, "...")) {
      *va_param = name;
      expect_token(tokens, ")");
      break;
    }

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

  bool is_like_func = is_funclike_macro_define_parens(token);
  if (is_like_func) {
    char* va_param = NULL;
    Str* params = funclike_macro_params(&token, &va_param);
    create_funclike_macro(name, params, va_param, macro_body(&token));
  } else {
    create_objlike_macro(name, macro_body(&token));
  }

  return token;
}

static Token* undef_dir(Token* token) {
  expect_dir(&token, "undef");

  Token* ident = expect_macro_ident_token(&token);
  char* name = strndup(ident->loc, ident->len);

  undefine_macro(name);

  return token;
}

char* compensate_include_filename(char* fname, char* path) {
  static Map cache;

  char* cached = get_from_map(&cache, fname);
  if (cached) {
    return cached;
  }

  if (start_with(fname, "/")) {
    put_to_map(&cache, fname, fname);
    return fname;
  }

  if (path) {
    char* compensated = format("%s/%s", path, fname);
    if (have_file(compensated)) {
      put_to_map(&cache, fname, compensated);
      last_included_path = path;
      return compensated;
    }
  }

  for (Str* path = include_paths.head.next; path; path = path->next) {
    char* compensated = format("%s/%s", path->data, fname);
    if (!have_file(compensated)) {
      continue;
    }

    put_to_map(&cache, fname, compensated);
    last_included_path = path->data;
    return compensated;
  }

  put_to_map(&cache, fname, fname);
  return fname;
}

static char* compensate_include_next_filename(char* fname) {
  for (Str* path = include_paths.head.next; path; path = path->next) {
    char* compensated = format("%s/%s", path->data, fname);
    if (!have_file(compensated)) {
      continue;
    }

    // [GNU] The include_next directive does not include the file in the last included directry.
    if (equal_to_str(path->data, last_included_path)) {
      continue;
    }

    return compensated;
  }

  return fname;
}

static char* include_filename(Token** tokens, bool* is_quoted) {
  // #include "..."
  if ((*tokens)->kind == TK_STR) {
    *is_quoted = true;

    // str_val cannot be used as a filename as it is escaped
    char* fname = strndup((*tokens)->loc + 1, (*tokens)->len - 2);
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

    return restore_contents(head.next);
  }

  // #include MACRO
  if ((*tokens)->kind == TK_IDENT) {
    Token* next = (*tokens)->next;
    Token* processed = append_tokens(process(copy_token(*tokens)), next);
    char* fname = include_filename(&processed, is_quoted);
    *tokens = processed;
    return fname;
  }

  error_token(*tokens, "expected a filename");
  return NULL;
}

static Token* skip_to_endif_dir(Token* token) {
  while (!is_dir(token, "endif")) {
    if (is_dir(token, "if") || is_dir(token, "ifdef") || is_dir(token, "ifndef")) {
      token = skip_to_endif_dir(token->next);
      expect_dir(&token, "endif");
      continue;
    }

    token = token->next;
  }
  return token;
}

static char* include_guard(Token* token) {
  if (!is_dir(token, "ifndef")) {
    return NULL;
  }
  expect_dir(&token, "ifndef");

  Token* cond = expect_macro_ident_token(&token);
  char* macro = strndup(cond->loc, cond->len);

  if (!is_dir(token, "define")) {
    return NULL;
  }
  expect_dir(&token, "define");

  Token* defined = expect_macro_ident_token(&token);
  if (!equal_to_n_chars(macro, defined->loc, defined->len)) {
    return NULL;
  }

  token = skip_to_endif_dir(token);
  expect_dir(&token, "endif");

  return token->kind == TK_EOF ? macro : NULL;
}

static Token* include_dir(Token* token) {
  expect_dir(&token, "include");

  bool is_quoted = false;
  char* fname = include_filename(&token, &is_quoted);
  fname = compensate_include_filename(fname, is_quoted ? dir(token->file->name) : NULL);

  // [GNU] A file which contains pragma once is not re-tokenized.
  if (get_from_map(&pragma_once, fname)) {
    return token;
  }

  // If the file has been tokenized once and the file has the include guard,
  // it it not necessary to re-tokenize it.
  char* guard = get_from_map(&include_guards, fname);
  if (guard && find_macro(guard)) {
    return token;
  }

  Token* included = tokenize(fname);
  guard = include_guard(included);
  if (guard) {
    put_to_map(&include_guards, fname, guard);
  }

  return append_tokens(included, token);
}

static Token* include_next_dir(Token* token) {
  expect_dir(&token, "include_next");

  bool ignored = false;
  char* fname = include_filename(&token, &ignored);
  fname = compensate_include_next_filename(fname);

  return append_tokens(tokenize(fname), token);
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
  Macro* macro = find_macro_by_token(ident);

  if (with_parens) {
    expect_token(tokens, ")");
  }

  return tokenize_as_if(start->file, start->line, format("%d", macro != NULL));
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
  cond = append_tokens(cond, new_eof_token_in(cond->file));
  cond = process(cond);

  for (Token* token = cond; token; token = token->next) {
    if (token->kind != TK_IDENT) {
      continue;
    }

    Token* next = token->next;
    *token = *tokenize_as_if(token->file, token->line, format("%d", 0));
    token->next = next;
  }

  cond = convert_tokens(cond);

  return const_expr(&cond) != 0;
}

static Token* ifdef_dir(Token* token) {
  Token* start = token;
  expect_dir(&token, "ifdef");

  Macro* macro = find_macro_by_token(token);
  token = token->next;
  IfDir* dir = create_if_dir(start, macro != NULL);
  if (!dir->cond) {
    token = skip_if_block(token);
  }

  return token;
}

static Token* ifndef_dir(Token* token) {
  Token* start = token;
  expect_dir(&token, "ifndef");

  Macro* macro = find_macro_by_token(token);
  token = token->next;
  IfDir* dir = create_if_dir(start, macro == NULL);
  if (!dir->cond) {
    token = skip_if_block(token);
  }

  return token;
}

static Token* if_dir(Token* token) {
  Token* start = token;
  expect_dir(&token, "if");

  IfDir* dir = create_if_dir(start, if_dir_cond(&token));
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

  return skip_extra_tokens(token);
}

static Token* line_dir(Token* tokens) {
  Token* start = tokens;

  if (is_hash(tokens) && tokens->next->kind == TK_PP_NUM) {
    expect_token(&tokens, "#");
  } else {
    expect_dir(&tokens, "line");
  }

  Token* line = append_tokens(inline_tokens(&tokens), new_eof_token_in(tokens->file));
  line = preprocess(line);

  if (line->kind != TK_NUM || line->type->kind != TY_INT) {
    error_token(line, "invalid line marker");
  }
  int delta = line->int_val - start->line;

  line = line->next;

  char* filename = line->kind == TK_STR ? line->str_val : start->file->name;

  for (Token* token = tokens; token; token = token->next) {
    token->file->name = filename;
    token->line += delta;
  }

  return tokens;
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

    // [GNU] include_next
    if (is_dir(token, "include_next")) {
      token->next = include_next_dir(token);
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

    if (is_dir(token, "line") || (is_hash(token) && token->next->kind == TK_PP_NUM)) {
      token->next = line_dir(token);
      continue;
    }

    if (is_dir(token, "error")) {
      error_token(token->next, "error");
    }

    // [GNU] pragma once
    if (is_dir(token, "pragma") && equal_to_token(token->next->next, "once")) {
      put_to_map(&pragma_once, token->file->name, (void*)true);
      inline_tokens(&token->next);
      continue;
    }

    if (is_dir(token, "pragma")) {
      inline_tokens(&token->next);
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

  return convert_tokens(tokens);
}