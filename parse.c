#include "cc.h"

typedef struct ScopedObj ScopedObj;
typedef struct Scope Scope;
typedef struct Initer Initer;
typedef struct DesignatedIniter DesignatedIniter;

struct ScopedObj {
  ScopedObj* next;
  char* key;
  Obj* obj;
};

struct Scope {
  Scope* next;
  ScopedObj* first_class_objs;
  ScopedObj* second_class_objs;
};

typedef struct VarAttr {
  bool is_typedef;
  bool is_extern;
  bool is_static;

  int alignment;
} VarAttr;

struct Initer {
  Initer* next;
  Type* type;
  Token* token;
  bool is_flexible;
  Node* expr;
  Initer** children;
};

struct DesignatedIniter {
  DesignatedIniter* next;
  int index;
  Obj* var;
  Member* mem;
};

static TopLevelObj* codes;
static Scope* gscope;
static Scope* current_scope;
static Obj* current_lvars;
static Node* current_switch;
static Node* current_labels;
static Node* current_gotos;
static char* current_break_label_id;
static char* current_continue_label_id;
static Obj* current_func;

static Type* new_type(TypeKind kind, int size, int alignment) {
  Type* type = calloc(1, sizeof(Type));
  type->kind = kind;
  type->size = size;
  type->alignment = alignment;
  return type;
}

static Type* new_void_type() {
  return new_type(TY_VOID, 1, 1);
}

static Type* new_bool_type() {
  return new_type(TY_BOOL, 1, 1);
}

static Type* new_char_type() {
  return new_type(TY_CHAR, 1, 1);
}

static Type* new_uchar_type() {
  Type* type = new_char_type();
  type->is_unsigned = true;
  return type;
}

static Type* new_short_type() {
  return new_type(TY_SHORT, 2, 2);
}

static Type* new_ushort_type() {
  Type* type = new_short_type();
  type->is_unsigned = true;
  return type;
}

Type* new_int_type() {
  return new_type(TY_INT, 4, 4);
}

Type* new_uint_type() {
  Type* type = new_int_type();
  type->is_unsigned = true;
  return type;
}

Type* new_long_type() {
  return new_type(TY_LONG, 8, 8);
}

Type* new_ulong_type() {
  Type* type = new_long_type();
  type->is_unsigned = true;
  return type;
}

Type* new_float_type() {
  return new_type(TY_FLOAT, 4, 4);
}

Type* new_double_type() {
  return new_type(TY_DOUBLE, 8, 8);
}

static char* new_id(void) {
  static int id = 0;
  char* name = calloc(20, sizeof(char));
  sprintf(name, ".L%d", id++);
  return name;
}

int align_up(int n, int align) {
  return (n + align - 1) / align * align;
}

static int align_down(int n, int align) {
  return align_up(n - align + 1, align);
}

static char* renew_label_id(char** current, char** next) {
  char* prev = *current;
  *next = *current = new_id();
  return prev;
}

static char* renew_break_label_id(char** next) {
  return renew_label_id(&current_break_label_id, next);
}

static char* renew_continue_label_id(char** next) {
  return renew_label_id(&current_continue_label_id, next);
}

static Type* new_ptr_type(Type* base) {
  Type* ptr = new_type(TY_PTR, 8, 8);
  ptr->base = base;
  ptr->is_unsigned = true;
  return ptr;
}

static Type* new_func_type(Type* return_type, Type* params, bool is_variadic) {
  Type* func = new_type(TY_FUNC, 0, 0);
  func->return_type = return_type;
  func->params = params;
  func->is_variadic = is_variadic;
  return func;
}

static Type* new_array_type(Type* base, int len) {
  Type* arr = new_type(TY_ARRAY, base->size * len, base->alignment);
  arr->base = base;
  arr->len = len;
  return arr;
}

static Type* new_chars_type(int len) {
  return new_array_type(new_char_type(), len);
}

static Member* new_member(Type* type) {
  Member* mem = calloc(1, sizeof(Member));
  mem->type = type;
  mem->token = type->ident;
  mem->name = type->name;
  mem->alignment = type->alignment;
  return mem;
}

static Member* copy_member(Member* src) {
  Member* mem = calloc(1, sizeof(Member));
  *mem = *src;
  return mem;
}

static Type* new_composite_type(TypeKind kind, int size, int alignment, Member* mems) {
  Type* type = new_type(kind, align_up(size, alignment), alignment);
  type->members = mems;
  return type;
}

static Type* new_struct_type(int size, int alignment, Member* mems) {
  return new_composite_type(TY_STRUCT, size, alignment, mems);
}

static Type* new_union_type(int size, int alignment, Member* mems) {
  return new_composite_type(TY_UNION, size, alignment, mems);
}

static Type* copy_type(Type* src) {
  Type* type = calloc(1, sizeof(Type));
  *type = *src;
  return type;
}

static Type* copy_type_with_name(Type* src, char* name) {
  Type* type = copy_type(src);
  type->name = name;
  return type;
}

static Type* copy_composite_type(Type* src, TypeKind kind) {
  Member head = {};
  Member* cur = &head;
  for (Member* mem = src->members; mem; mem = mem->next) {
    cur = cur->next = copy_member(mem);
  }

  return new_composite_type(kind, src->size, src->alignment, head.next);
}

static Type* inherit_decl(Type* dst, Type* src) {
  dst->ident = src->ident;
  dst->name = src->name;
  return dst;
}

static Type* get_common_type(Type* a, Type* b) {
  if (a->kind == TY_FUNC) {
    return new_ptr_type(a);
  }
  if (b->kind == TY_FUNC) {
    return new_ptr_type(b);
  }

  if (a->base) {
    return new_ptr_type(a->base);
  }

  if (a->kind == TY_DOUBLE || b->kind == TY_DOUBLE) {
    return new_double_type();
  }
  if (a->kind == TY_FLOAT || b->kind == TY_FLOAT) {
    return new_float_type();
  }

  Type* x = a;
  Type* y = b;

  if (x->size < 4) {
    x = new_int_type();
  }
  if (y->size < 4) {
    y = new_int_type();
  }

  if (x->size != y->size) {
    return x->size > y->size ? x : y;
  }

  return y->is_unsigned ? y : x;
}

static Type* deref_ptr_type(Type* type) {
  return type->kind == TY_PTR ? type->base : type;
}

static bool is_pointable_type(Type* type) {
  return type->kind == TY_PTR || type->kind == TY_ARRAY;
}

bool is_int_type(Type* type) {
  return type->kind == TY_BOOL || type->kind == TY_CHAR || type->kind == TY_SHORT || type->kind == TY_INT
         || type->kind == TY_LONG;
}

bool is_float_type(Type* type) {
  return type->kind == TY_DOUBLE || type->kind == TY_FLOAT;
}

static bool is_numeric_type(Type* type) {
  return is_int_type(type) || is_float_type(type);
}

bool is_composite_type(Type* type) {
  return type->kind == TY_STRUCT || type->kind == TY_UNION;
}

static TopLevelObj* new_top_level_obj(Obj* obj) {
  TopLevelObj* objs = calloc(1, sizeof(TopLevelObj));
  objs->obj = obj;
  return objs;
}

static void add_code(Obj* code) {
  if (code->kind != OJ_FUNC && code->kind != OJ_GVAR) {
    error("expected a top level object");
  }

  TopLevelObj* top_level = new_top_level_obj(code);
  top_level->next = codes;
  codes = top_level;
}

static ScopedObj* new_scoped_obj(char* key, Obj* obj) {
  ScopedObj* scoped = calloc(1, sizeof(ScopedObj));
  scoped->key = key;
  scoped->obj = obj;
  return scoped;
}

static void enter_scope(void) {
  Scope* next = calloc(1, sizeof(Scope));
  next->next = current_scope;
  current_scope = next;
}

static void leave_scope(void) {
  if (!current_scope) {
    error("no scope to leave");
  }
  current_scope = current_scope->next;
}

static void add_first_class_obj_to_scope(Scope* scope, char* key, Obj* obj) {
  switch (obj->kind) {
    case OJ_FUNC:
    case OJ_GVAR:
    case OJ_LVAR:
    case OJ_ENUM:
    case OJ_DEF_TYPE: {
      ScopedObj* scoped = new_scoped_obj(key, obj);
      scoped->next = scope->first_class_objs;
      scope->first_class_objs = scoped;
      break;
    }
    default:
      error("expected a first class object");
  }
}

static void add_second_class_obj_to_scope(Scope* scope, char* key, Obj* obj) {
  if (obj->kind != OJ_TAG) {
    error("expected a second class object");
  }

  ScopedObj* scoped = new_scoped_obj(key, obj);
  scoped->next = scope->second_class_objs;
  scope->second_class_objs = scoped;
}

static void add_func_obj_to_scope(Scope* scope, char* key, Obj* func) {
  if (func->kind != OJ_FUNC) {
    error("expected a function");
  }

  add_first_class_obj_to_scope(scope, key, func);
}

static void add_var_obj_to_scope(Scope* scope, char* key, Obj* var) {
  if (var->kind != OJ_GVAR && var->kind != OJ_LVAR) {
    error("expected a variable");
  }

  add_first_class_obj_to_scope(scope, key, var);
}

static void add_enum_obj_to_scope(Scope* scope, char* key, Obj* enu) {
  if (enu->kind != OJ_ENUM) {
    error("expected an enum");
  }

  add_first_class_obj_to_scope(scope, key, enu);
}

static void add_def_type_obj_to_scope(Scope* scope, char* key, Obj* def_type) {
  if (def_type->kind != OJ_DEF_TYPE) {
    error("expected a defined type");
  }

  add_first_class_obj_to_scope(scope, key, def_type);
}

static void add_tag_obj_to_scope(Scope* scope, char* key, Obj* tag) {
  if (tag->kind != OJ_TAG) {
    error("expected a tag");
  }

  add_second_class_obj_to_scope(scope, key, tag);
}

static bool is_gscope(Scope* scope) {
  return !scope->next;
}

static void add_func_obj(Obj* func) {
  if (func->kind != OJ_FUNC) {
    error("expected a function");
  }

  add_func_obj_to_scope(current_scope, func->name, func);
}

static void add_gvar_obj(Obj* var) {
  if (var->kind != OJ_GVAR) {
    error("expected a global var");
  }
  if (!is_gscope(gscope)) {
    error("expected a global scope");
  }

  add_var_obj_to_scope(gscope, var->name, var);
}

static void add_var_obj_to_current_local_scope(char* key, Obj* var) {
  if (is_gscope(current_scope)) {
    error("expected a local scope");
  }

  int current_offset = current_lvars ? current_lvars->offset : 0;
  if (var->offset < current_offset) {
    error("expected the variable offset to be adjusted");
  }

  var->next = current_lvars;
  current_lvars = var;
  add_var_obj_to_scope(current_scope, key, var);
}

static void add_lvar_obj(Obj* var) {
  if (var->kind != OJ_LVAR) {
    error("expected a local var");
  }

  add_var_obj_to_current_local_scope(var->name, var);
}

static void add_tag_obj(Obj* tag) {
  if (tag->kind != OJ_TAG) {
    error("expected a tag");
  }

  add_tag_obj_to_scope(current_scope, tag->name, tag);
}

static void add_def_type_obj(Obj* def_type) {
  if (def_type->kind != OJ_DEF_TYPE) {
    error("expected a defined type");
  }

  add_def_type_obj_to_scope(current_scope, def_type->name, def_type);
}

static void add_enum_obj(Obj* enu) {
  if (enu->kind != OJ_ENUM) {
    error("expected an enum");
  }

  add_enum_obj_to_scope(current_scope, enu->name, enu);
}

static Obj* new_obj(ObjKind kind) {
  Obj* obj = calloc(1, sizeof(Obj));
  obj->kind = kind;
  return obj;
}

static Obj* create_func_obj(Type* type, char* name) {
  Obj* func = new_obj(OJ_FUNC);
  func->type = type;
  func->name = name;
  add_func_obj(func);
  add_code(func);
  return func;
}

static Obj* prepare_gvar_obj(Type* type, char* name) {
  Obj* var = new_obj(OJ_GVAR);
  var->type = type;
  var->name = name;
  var->is_definition = true;
  var->alignment = type->alignment;
  return var;
}

static Obj* create_stray_gvar_obj(Type* type, char* name) {
  Obj* var = prepare_gvar_obj(type, name);
  add_gvar_obj(var);
  return var;
}

static Obj* create_gvar_obj(Type* type, char* name) {
  Obj* var = create_stray_gvar_obj(type, name);
  add_code(var);
  return var;
}

static Obj* create_anon_gvar_obj(Type* type) {
  return create_gvar_obj(type, new_id());
}

static Obj* create_str_obj(char* val, int len) {
  Type* type = new_chars_type(len + 1);
  Obj* str = create_stray_gvar_obj(type, new_id());
  str->is_static = true;
  str->val = strdup(val);
  add_code(str);
  return str;
}

static void adjust_lvar_obj_offset(Obj* vars, Obj* var, Type* type) {
  if (var->kind != OJ_LVAR) {
    error("expected a local var");
  }

  var->type = type;
  // If the type has not been determined yet (type->size < 0),
  // treat its size as 0
  int size = var->type->size >= 0 ? var->type->size : 0;
  var->offset = align_up(vars ? vars->offset + size : size, var->alignment);
}

static Obj* create_static_lvar_obj(Type* type, char* name) {
  Obj* var = create_anon_gvar_obj(type);
  var->is_static = true;
  // Inherit the current offset so that other lvars can extend their offset on it
  var->offset = current_lvars ? current_lvars->offset : 0;
  add_var_obj_to_current_local_scope(name, var);
  return var;
}

static Obj* create_static_str_lvar_obj(char* name, char* s) {
  Type* type = copy_type_with_name(new_chars_type(strlen(s) + 1), name);
  Obj* obj = create_static_lvar_obj(type, name);
  obj->val = strdup(s);
  return obj;
}

static Obj* create_lvar_obj(Type* type, char* name) {
  Obj* var = new_obj(OJ_LVAR);
  var->type = type;
  var->name = name;
  var->alignment = type->alignment;
  adjust_lvar_obj_offset(current_lvars, var, type);
  add_lvar_obj(var);
  return var;
}

static Obj* create_anon_lvar_obj(Type* type) {
  return create_lvar_obj(type, "");
}

static Obj* create_enum_obj(char* name, int64_t val) {
  Obj* enu = new_obj(OJ_ENUM);
  enu->name = name;
  // NOLINTNEXTLINE
  enu->val = (char*)val;
  add_enum_obj(enu);
  return enu;
}

static Obj* create_def_type_obj(Type* type, char* name) {
  Obj* def_type = new_obj(OJ_DEF_TYPE);
  def_type->type = type;
  def_type->name = name;
  add_def_type_obj(def_type);
  return def_type;
}

static Obj* create_tag_obj(Type* type, char* name) {
  Obj* tag = new_obj(OJ_TAG);
  tag->type = type;
  tag->name = name;
  add_tag_obj(tag);
  return tag;
}

static Obj* find_datum_obj(char* name, int len) {
  for (Scope* s = current_scope; s; s = s->next) {
    for (ScopedObj* var = s->first_class_objs; var; var = var->next) {
      if (!equal_to_n_chars(var->key, name, len)) {
        continue;
      }

      switch (var->obj->kind) {
        case OJ_FUNC:
        case OJ_GVAR:
        case OJ_LVAR:
        case OJ_ENUM:
          return var->obj;
        default:
          return NULL;
      }
    }
  }
  return NULL;
}

static Obj* find_def_type_obj(char* name, int len) {
  for (Scope* s = current_scope; s; s = s->next) {
    for (ScopedObj* def_type = s->first_class_objs; def_type; def_type = def_type->next) {
      if (!equal_to_n_chars(def_type->key, name, len)) {
        continue;
      }

      return def_type->obj->kind == OJ_DEF_TYPE ? def_type->obj : NULL;
    }
  }
  return NULL;
}

static Obj* find_tag_obj_in_current_scope(char* name, int len) {
  for (ScopedObj* tag = current_scope->second_class_objs; tag; tag = tag->next) {
    if (!equal_to_n_chars(tag->key, name, len)) {
      continue;
    }

    return tag->obj->kind == OJ_TAG ? tag->obj : NULL;
  }
  return NULL;
}

static Obj* find_tag_obj(char* name, int len) {
  for (Scope* s = current_scope; s; s = s->next) {
    for (ScopedObj* tag = s->second_class_objs; tag; tag = tag->next) {
      if (!equal_to_n_chars(tag->key, name, len)) {
        continue;
      }

      return tag->obj->kind == OJ_TAG ? tag->obj : NULL;
    }
  }
  return NULL;
}

static bool equal_to_decl_specifier(Token* token) {
  static char* names[] = {
    "typedef",
    "extern",
    "static",
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
  static int len = sizeof(names) / sizeof(char*);

  for (int i = 0; i < len; i++) {
    if (equal_to_token(token, names[i])) {
      return true;
    }
  }

  return find_def_type_obj(token->loc, token->len) != NULL;
}

static bool equal_to_abstract_decl_start(Token* token) {
  return equal_to_token(token, "(") && equal_to_decl_specifier(token->next);
}

static void add_label(Node* l) {
  if (l->kind != ND_LABEL) {
    error("expected a label");
  }

  l->labels = current_labels;
  current_labels = l;
}

static void add_goto(Node* g) {
  if (g->kind != ND_GOTO) {
    error("expected a goto");
  }

  g->gotos = current_gotos;
  current_gotos = g;
}

static Node* new_node(NodeKind kind) {
  Node* node = calloc(1, sizeof(Node));
  node->kind = kind;
  return node;
}

static Node* new_unary_node(NodeKind kind, Node* lhs) {
  Node* node = new_node(kind);
  node->token = lhs->token;
  node->lhs = lhs;
  return node;
}

static Node* new_binary_node(NodeKind kind, Node* lhs, Node* rhs) {
  Node* node = new_node(kind);
  node->token = lhs->token;
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

static Node* new_stmt_expr_node(Token* token, Node* body) {
  Node* last_stmt = NULL;
  Node* stmt = body;
  while (stmt) {
    last_stmt = stmt;
    stmt = stmt->next;
  }
  if (!last_stmt || !last_stmt->type) {
    error_token(token, "statement expression returning void is not supported");
  }

  Node* expr = new_node(ND_STMT_EXPR);
  expr->type = last_stmt->type;
  expr->token = token;
  expr->body = body;
  return expr;
}

static Node* new_func_node(Type* type, Token* token, char* name, bool is_definition) {
  Node* node = new_node(ND_FUNC);
  node->type = type;
  node->token = token;
  node->name = name;
  node->is_definition = is_definition;
  return node;
}

static Node* new_gvar_node(Token* token, Obj* obj) {
  Node* node = new_node(ND_GVAR);
  node->type = obj->type;
  node->token = token;
  node->name = obj->name;
  node->obj = obj;
  return node;
}

static Node* new_lvar_node(Token* token, Obj* obj) {
  Node* node = new_node(ND_LVAR);
  node->type = obj->type;
  node->token = token;
  node->obj = obj;
  return node;
}

static Node* new_var_node(Token* token, Obj* obj) {
  switch (obj->kind) {
    case OJ_GVAR:
      return new_gvar_node(token, obj);
    case OJ_LVAR:
      return new_lvar_node(token, obj);
    default:
      error_token(token, "expected a variable object");
      return NULL;
  }
}

static Node* new_funccall_node(Token* token, Node* lhs, Node* args) {
  Node* node = new_unary_node(ND_FUNCCALL, lhs);
  node->type = deref_ptr_type(lhs->type)->return_type;
  node->token = token;
  node->args = args;
  if (is_composite_type(node->type)) {
    node->return_val = new_lvar_node(token, create_anon_lvar_obj(node->type));
  }
  return node;
}

static Node* new_member_node(Token* token, Node* lhs, Member* mem) {
  Node* node = new_unary_node(ND_MEMBER, lhs);
  node->type = mem->type;
  node->token = token;
  node->name = mem->name;
  node->mem = mem;
  return node;
}

static Node* new_int_node(Token* token, int64_t val) {
  Node* node = new_node(ND_NUM);
  node->type = token->type ? token->type : new_int_type();
  node->token = token;
  node->int_val = val;
  return node;
}

static Node* new_long_node(Token* token, int64_t val) {
  Node* node = new_node(ND_NUM);
  node->type = new_long_type();
  node->token = token;
  node->int_val = val;
  return node;
}

static Node* new_ulong_node(Token* token, int64_t val) {
  Node* node = new_node(ND_NUM);
  node->type = new_ulong_type();
  node->token = token;
  node->int_val = val;
  return node;
}

static Node* new_float_node(Token* token, double val) {
  Node* node = new_node(ND_NUM);
  node->type = token->type ? token->type : new_double_type();
  node->token = token;
  node->float_val = val;
  return node;
}

static Node* new_null_node(Token* token) {
  Node* node = new_node(ND_NULL);
  node->token = token;
  return node;
}

static Node* new_memzero_node(Token* token, Obj* obj) {
  Node* node = new_node(ND_MEMZERO);
  node->type = obj->type;
  node->token = token;
  node->obj = obj;
  return node;
}

static Node* new_neg_node(Token* token, Node* lhs) {
  Node* neg = new_unary_node(ND_NEG, lhs);
  neg->type = lhs->type;
  neg->token = token;
  return neg;
}

static Node* new_addr_node(Token* token, Node* lhs) {
  Node* addr = new_unary_node(ND_ADDR, lhs);
  if (addr->lhs->type->kind == TY_ARRAY) {
    addr->type = new_ptr_type(addr->lhs->type->base);
  } else {
    addr->type = new_ptr_type(addr->lhs->type);
  }
  addr->token = token;
  return addr;
}

static Node* new_deref_node(Token* token, Node* lhs) {
  Node* deref = new_unary_node(ND_DEREF, lhs);
  deref->type = deref->lhs->type->base ? deref->lhs->type->base : deref->lhs->type;
  deref->token = token;
  return deref;
}

static Node* new_not_node(Token* token, Node* lhs) {
  Node* n = new_unary_node(ND_NOT, lhs);
  n->type = new_int_type();
  n->token = token;
  return n;
}

static Node* new_bitnot_node(Token* token, Node* lhs) {
  Node* n = new_unary_node(ND_BITNOT, lhs);
  n->type = lhs->type;
  n->token = token;
  return n;
}

static Node* new_cast_node(Type* type, Token* token, Node* lhs) {
  Node* node = new_node(ND_CAST);
  node->type = type;
  node->token = token;
  node->lhs = lhs;
  return node;
}

static void usual_arith_convert(Node** lhs, Node** rhs) {
  Type* common = get_common_type((*lhs)->type, (*rhs)->type);
  *lhs = new_cast_node(common, (*lhs)->token, *lhs);
  *rhs = new_cast_node(common, (*rhs)->token, *rhs);
}

static Node* new_mul_node(Token* token, Node* lhs, Node* rhs) {
  usual_arith_convert(&lhs, &rhs);
  Node* mul = new_binary_node(ND_MUL, lhs, rhs);
  mul->token = token;
  mul->type = lhs->type;
  return mul;
}

static Node* new_div_node(Token* token, Node* lhs, Node* rhs) {
  usual_arith_convert(&lhs, &rhs);
  Node* div = new_binary_node(ND_DIV, lhs, rhs);
  div->token = token;
  div->type = lhs->type;
  return div;
}

static Node* new_mod_node(Token* token, Node* lhs, Node* rhs) {
  usual_arith_convert(&lhs, &rhs);
  Node* mod = new_binary_node(ND_MOD, lhs, rhs);
  mod->token = token;
  mod->type = lhs->type;
  return mod;
}

static Node* new_add_node(Token* token, Node* lhs, Node* rhs) {
  // ptr + ptr
  if (is_pointable_type(lhs->type) && is_pointable_type(rhs->type)) {
    error_token(token, "invalid operands");
  }

  // num + num
  if (is_numeric_type(lhs->type) && is_numeric_type(rhs->type)) {
    usual_arith_convert(&lhs, &rhs);
    Node* add = new_binary_node(ND_ADD, lhs, rhs);
    add->token = token;
    add->type = add->lhs->type;
    return add;
  }

  // ptr + num
  if (is_pointable_type(lhs->type) && is_int_type(rhs->type)) {
    rhs = new_mul_node(rhs->token, rhs, new_long_node(rhs->token, lhs->type->base->size));
    Node* add = new_binary_node(ND_ADD, lhs, rhs);
    add->token = token;
    add->type = add->lhs->type;
    return add;
  }

  // num + ptr
  lhs = new_mul_node(lhs->token, lhs, new_long_node(lhs->token, rhs->type->base->size));
  Node* add = new_binary_node(ND_ADD, lhs, rhs);
  add->token = token;
  add->type = add->rhs->type;
  return add;
}

static Node* new_sub_node(Token* token, Node* lhs, Node* rhs) {
  // num - ptr
  if (is_int_type(lhs->type) && is_pointable_type(rhs->type)) {
    error_token(token, "invalid operands");
  }

  // ptr - ptr
  if (is_pointable_type(lhs->type) && is_pointable_type(rhs->type)) {
    Node* sub = new_binary_node(ND_SUB, lhs, rhs);
    sub->token = token;
    sub->type = new_long_type();
    return new_div_node(sub->token, sub, new_int_node(lhs->token, sub->lhs->type->base->size));
  }

  // num - num
  if (is_numeric_type(lhs->type) && is_numeric_type(rhs->type)) {
    usual_arith_convert(&lhs, &rhs);
    Node* sub = new_binary_node(ND_SUB, lhs, rhs);
    sub->token = token;
    sub->type = sub->lhs->type;
    return sub;
  }

  // ptr - num
  rhs = new_mul_node(rhs->token, rhs, new_long_node(rhs->token, lhs->type->base->size));
  Node* sub = new_binary_node(ND_SUB, lhs, rhs);
  sub->token = token;
  sub->type = sub->lhs->type;
  return sub;
}

static Node* new_lshift_node(Token* token, Node* lhs, Node* rhs) {
  Node* shift = new_binary_node(ND_LSHIFT, lhs, rhs);
  shift->type = lhs->type;
  shift->token = token;
  return shift;
}

static Node* new_rshift_node(Token* token, Node* lhs, Node* rhs) {
  Node* shift = new_binary_node(ND_RSHIFT, lhs, rhs);
  shift->type = lhs->type;
  shift->token = token;
  return shift;
}

static Node* new_eq_node(Token* token, Node* lhs, Node* rhs) {
  usual_arith_convert(&lhs, &rhs);
  Node* eq = new_binary_node(ND_EQ, lhs, rhs);
  eq->token = token;
  eq->type = new_long_type();
  return eq;
}

static Node* new_ne_node(Token* token, Node* lhs, Node* rhs) {
  usual_arith_convert(&lhs, &rhs);
  Node* ne = new_binary_node(ND_NE, lhs, rhs);
  ne->token = token;
  ne->type = new_long_type();
  return ne;
}

static Node* new_lt_node(Token* token, Node* lhs, Node* rhs) {
  usual_arith_convert(&lhs, &rhs);
  Node* lt = new_binary_node(ND_LT, lhs, rhs);
  lt->token = token;
  lt->type = new_long_type();
  return lt;
}

static Node* new_le_node(Token* token, Node* lhs, Node* rhs) {
  usual_arith_convert(&lhs, &rhs);
  Node* le = new_binary_node(ND_LE, lhs, rhs);
  le->token = token;
  le->type = new_long_type();
  return le;
}

static Node* new_bitand_node(Token* token, Node* lhs, Node* rhs) {
  usual_arith_convert(&lhs, &rhs);
  Node* n = new_binary_node(ND_BITAND, lhs, rhs);
  n->token = token;
  n->type = lhs->type;
  return n;
}

static Node* new_bitxor_node(Token* token, Node* lhs, Node* rhs) {
  usual_arith_convert(&lhs, &rhs);
  Node* n = new_binary_node(ND_BITXOR, lhs, rhs);
  n->token = token;
  n->type = lhs->type;
  return n;
}

static Node* new_bitor_node(Token* token, Node* lhs, Node* rhs) {
  usual_arith_convert(&lhs, &rhs);
  Node* n = new_binary_node(ND_BITOR, lhs, rhs);
  n->token = token;
  n->type = lhs->type;
  return n;
}

static Node* new_and_node(Token* token, Node* lhs, Node* rhs) {
  usual_arith_convert(&lhs, &rhs);
  Node* n = new_binary_node(ND_LOGAND, lhs, rhs);
  n->token = token;
  n->type = new_int_type();
  return n;
}

static Node* new_or_node(Token* token, Node* lhs, Node* rhs) {
  usual_arith_convert(&lhs, &rhs);
  Node* n = new_binary_node(ND_LOGOR, lhs, rhs);
  n->token = token;
  n->type = new_int_type();
  return n;
}

static Node* new_cond_node(Token* token, Node* cond, Node* then, Node* els) {
  Node* c = new_node(ND_COND);
  c->token = token;
  c->cond = cond;
  c->then = then;
  c->els = els;

  if (c->then->type->kind == TY_VOID || c->els->type->kind == TY_VOID) {
    c->type = new_void_type();
  } else {
    usual_arith_convert(&c->then, &c->els);
    c->type = c->then->type;
  }
  return c;
}

static Node* new_assign_node(Token* token, Node* lhs, Node* rhs) {
  if (lhs->type->kind != TY_STRUCT) {
    rhs = new_cast_node(lhs->type, rhs->token, rhs);
  }
  Node* assign = new_binary_node(ND_ASSIGN, lhs, rhs);
  assign->token = token;
  assign->type = lhs->type;
  return assign;
}

static Node* new_shorthand_assign_node(Token* token, NodeKind op, Node* lhs, Node* rhs) {
  Node* node = NULL;
  switch (op) {
    case ND_BITOR:
      node = new_bitor_node(token, lhs, rhs);
      break;
    case ND_BITXOR:
      node = new_bitxor_node(token, lhs, rhs);
      break;
    case ND_BITAND:
      node = new_bitand_node(token, lhs, rhs);
      break;
    case ND_LSHIFT:
      node = new_lshift_node(token, lhs, rhs);
      break;
    case ND_RSHIFT:
      node = new_rshift_node(token, lhs, rhs);
      break;
    case ND_ADD:
      node = new_add_node(token, lhs, rhs);
      break;
    case ND_SUB:
      node = new_sub_node(token, lhs, rhs);
      break;
    case ND_MUL:
      node = new_mul_node(token, lhs, rhs);
      break;
    case ND_DIV:
      node = new_div_node(token, lhs, rhs);
      break;
    case ND_MOD:
      node = new_mod_node(token, lhs, rhs);
      break;
    default:
      error_token(token, "invalid operation");
  }

  return new_assign_node(lhs->token, lhs, node);
}

static Node* new_comma_node(Token* token, Node* lhs, Node* rhs) {
  Node* comma = new_binary_node(ND_COMMA, lhs, rhs);
  comma->token = token;
  comma->type = comma->rhs->type;
  return comma;
}

static Node* new_expr_stmt_node(Node* lhs) {
  Node* stmt = new_unary_node(ND_EXPR_STMT, lhs);
  stmt->type = lhs->type;
  return stmt;
}

static Node* create_label_node(Token* token, char* label, Node* lhs) {
  Node* l = new_unary_node(ND_LABEL, lhs);
  l->token = token;
  l->label = label;
  l->label_id = new_id();
  add_label(l);
  return l;
}

static Node* create_goto_node(Token* token, char* label) {
  Node* g = new_node(ND_GOTO);
  g->token = token;
  g->label = label;
  add_goto(g);
  return g;
}

static Node* new_contextual_goto_node(Token* token, char* label_id) {
  Node* node = new_node(ND_GOTO);
  node->token = token;
  node->label_id = label_id;
  return node;
}

static Node* new_block_node(Token* token, Node* body) {
  Node* node = new_node(ND_BLOCK);
  node->token = token;
  node->body = body;
  return node;
}

static void func(Token** tokens);
static void gvar(Token** tokens);
static void tydef(Token** tokens);
static Node* block_stmt(Token** tokens);
static Node* stmt(Token** tokens);
static Node* expr_stmt(Token** tokens);
static Node* expr(Token** tokens);
static Node* assign(Token** tokens);
static Node* conditional(Token** tokens);
static Node* log_or(Token** tokens);
static Node* log_and(Token** tokens);
static Node* bit_or(Token** tokens);
static Node* bit_xor(Token** tokens);
static Node* bit_and(Token** tokens);
static Node* equality(Token** tokens);
static Node* relational(Token** tokens);
static Node* shift(Token** tokens);
static Node* add(Token** tokens);
static Node* mul(Token** tokens);
static Node* cast(Token** tokens);
static Node* unary(Token** tokens);
static Node* postfix(Token** tokens);
static Node* compound_literal(Token** tokens);
static Node* primary(Token** tokens);
static Node* lvar(Token** tokens);
static Node* lvar_decl(Token** tokens);
static Type* enum_specifier(Token** tokens);
static Type* struct_decl(Token** tokens);
static Type* union_decl(Token** tokens);
static Member* members(Token** tokens, bool* is_flexible);
static Type* decl_specifier(Token** tokens, VarAttr* attr);
static Type* declarator(Token** tokens, Type* type);
static Type* decl(Token** tokens, VarAttr* attr);
static Type* abstract_declarator(Token** tokens, Type* type);
static Type* abstract_decl(Token** tokens, VarAttr* attr);
static Type* type_suffix(Token** tokens, Type* type);
static Type* func_params(Token** tokens, Type* type);
static Type* array_dimensions(Token** tokens, Type* type);
static Node* func_args(Token** tokens, Type* type);
static int64_t eval(Node* node);
static double eval_float(Node* node);
static int64_t eval_reloc(Node* node, char** label);
static void gvar_initer(Token** tokens, Obj* var);
static Node* lvar_initer(Token** tokens, Obj* var);
static Initer* initer(Token** tokens, Type** type);

bool is_func_declarator(Token* token, Type* type) {
  if (equal_to_token(token, ";")) {
    return false;
  }

  type = declarator(&token, type);
  return type->kind == TY_FUNC;
}

TopLevelObj* parse(Token* token) {
  enter_scope();
  gscope = current_scope;

  while (token->kind != TK_EOF) {
    Token* peeked = token;
    VarAttr attr = {};
    Type* spec = decl_specifier(&peeked, &attr);

    if (attr.is_typedef) {
      tydef(&token);
      continue;
    }

    if (is_func_declarator(peeked, spec)) {
      func(&token);
      continue;
    }

    gvar(&token);
  }

  leave_scope();

  return codes;
}

static void resolve_goto_labels(void) {
  for (Node* g = current_gotos; g; g = g->gotos) {
    for (Node* l = current_labels; l; l = l->labels) {
      if (!equal_to_str(g->label, l->label)) {
        continue;
      }

      g->label_id = l->label_id;
      break;
    }

    if (g->label_id == NULL) {
      error_token(g->token->next, "use of undeclared label");
    }
  }

  current_labels = NULL;
  current_gotos = NULL;
}

static Node* new_func_params(Type* params) {
  Node head = {};
  Node* cur = &head;
  for (Type* param = params; param; param = param->next) {
    if (!param->name) {
      error_token(param->ident, "parameter name omitted");
    }

    Obj* var = create_lvar_obj(param, param->name);
    cur = cur->next = new_var_node(param->ident, var);
  }

  return head.next;
}

static void func(Token** tokens) {
  VarAttr attr = {};
  Type* type = decl(tokens, &attr);
  if (!type->name) {
    error_token(type->ident, "function name omitted");
  }

  Obj* func = create_func_obj(type, type->name);
  current_func = func;

  func->is_static = attr.is_static;
  func->is_definition = !consume_token(tokens, ";");
  if (!func->is_definition) {
    current_lvars = NULL;
    current_func = NULL;
    return;
  }

  enter_scope();

  // The return values whose types are struct or union and whose size are larger than 16 bytes
  // are returned by the callee's filling those values via the pointers to those values,
  // so assign the stack of the callee for the pointers.
  if (is_composite_type(func->type->return_type) && func->type->return_type->size > 16) {
    func->ptr_to_return_val = new_var_node(*tokens, create_anon_lvar_obj(new_ptr_type(func->type->return_type)));
  }

  func->params = new_func_params(func->type->params);

  if (func->type->is_variadic) {
    func->va_area = create_lvar_obj(new_chars_type(136), "__va_area__");
  }

  create_static_str_lvar_obj("__func__", func->name);
  create_static_str_lvar_obj("__FUNCTION__", func->name);

  func->body = block_stmt(tokens);

  leave_scope();

  func->lvars = current_lvars;
  func->stack_size = align_up(func->lvars ? func->lvars->offset : 0, 16);

  current_lvars = NULL;
  resolve_goto_labels();
  current_func = NULL;
}

// NOLINTNEXTLINE
static uint64_t read_uint_data(char* loc, int size) {
  switch (size) {
    case 1:
      return *loc;
    case 2:
      return *(uint16_t*)loc;
    case 4:
      return *(uint32_t*)loc;
    default:
      return *(uint64_t*)loc;
  }
}

static void write_int_data(char* loc, int size, uint64_t val) {
  switch (size) {
    case 1:
      *loc = val;
      return;
    case 2:
      *(uint16_t*)loc = val;
      return;
    case 4:
      *(uint32_t*)loc = val;
      return;
    default:
      *(uint64_t*)loc = val;
      return;
  }
}

static void write_float_data(char* loc, Type* type, double val) {
  switch (type->kind) {
    case TY_FLOAT:
      *(float*)loc = val;
      return;
    default:
      *(double*)loc = val;
      return;
  }
}

static Relocation* new_reloc(char* label, int offset, long addend) {
  Relocation* reloc = calloc(1, sizeof(Relocation));
  reloc->label = label;
  reloc->offset = offset;
  reloc->addend = addend;
  return reloc;
}

static Relocation* write_gvar_data(char* data, int offset, Initer* init, Relocation* reloc) {
  switch (init->type->kind) {
    case TY_STRUCT: {
      int i = 0;
      for (Member* mem = init->type->members; mem; mem = mem->next) {
        if (mem->is_bitfield) {
          Node* expr = init->children[i]->expr;
          if (!expr) {
            break;
          }

          char* loc = data + offset + mem->offset;
          uint64_t cur = read_uint_data(loc, mem->type->size);
          uint64_t val = eval(expr);
          uint64_t mask = (1L << mem->bit_width) - 1;
          val = cur | (val & mask) << mem->bit_offset;
          write_int_data(loc, mem->type->size, val);
        } else {
          reloc = write_gvar_data(data, offset + mem->offset, init->children[i], reloc);
        }

        i++;
      }
      return reloc;
    }
    case TY_UNION:
      return write_gvar_data(data, offset, init->children[0], reloc);
    case TY_ARRAY: {
      int size = init->type->base->size;
      for (int i = 0; i < init->type->len; i++) {
        reloc = write_gvar_data(data, offset + size * i, init->children[i], reloc);
      }
      return reloc;
    }
    default: {
      if (!init->expr) {
        return reloc;
      }

      if (is_float_type(init->type)) {
        write_float_data(data + offset, init->type, eval_float(init->expr));
        return reloc;
      }

      char* label = NULL;
      uint64_t val = eval_reloc(init->expr, &label);
      if (!label) {
        write_int_data(data + offset, init->type->size, val);
        return reloc;
      }

      reloc->next = new_reloc(label, offset, val);
      return reloc->next;
    }
  }
}

static void gvar_initer(Token** tokens, Obj* var) {
  Initer* init = initer(tokens, &var->type);
  Relocation relocs = {};
  char* data = calloc(1, var->type->size);
  write_gvar_data(data, 0, init, &relocs);
  var->relocs = relocs.next;
  var->val = data;
}

static void gvar(Token** tokens) {
  VarAttr attr = {};
  Type* spec = decl_specifier(tokens, &attr);

  bool is_first = true;
  while (!consume_token(tokens, ";")) {
    if (!is_first) {
      expect_token(tokens, ",");
    }
    is_first = false;

    Type* type = declarator(tokens, spec);
    if (!type->name) {
      error_token(type->ident, "variable name omitted");
    }
    if (attr.alignment) {
      type = copy_type(type);
      type->alignment = attr.alignment;
    }

    Obj* var = create_gvar_obj(type, type->name);
    var->is_static = attr.is_static;
    var->is_definition = !attr.is_extern;

    if (consume_token(tokens, "=")) {
      gvar_initer(tokens, var);
    }
  }
}

static void tydef(Token** tokens) {
  expect_token(tokens, "typedef");

  Type* spec = decl_specifier(tokens, NULL);

  bool is_first = true;
  while (!consume_token(tokens, ";")) {
    if (!is_first) {
      expect_token(tokens, ",");
    }
    is_first = false;

    Type* type = declarator(tokens, spec);
    if (!type->name) {
      error_token(type->ident, "typedef name omitted");
    }
    create_def_type_obj(type, type->name);
  }
}

static Node* block_stmt(Token** tokens) {
  Token* start = *tokens;

  expect_token(tokens, "{");
  enter_scope();

  Node head = {};
  Node* cur = &head;
  while (!consume_token(tokens, "}")) {
    if (equal_to_decl_specifier(*tokens) && !equal_to_token((*tokens)->next, ":")) {
      Token* peeked = *tokens;
      VarAttr attr = {};
      Type* spec = decl_specifier(&peeked, &attr);

      if (attr.is_typedef) {
        tydef(tokens);
        continue;
      }

      if (is_func_declarator(peeked, spec)) {
        func(tokens);
        continue;
      }

      if (attr.is_extern) {
        gvar(tokens);
        continue;
      }

      cur = cur->next = lvar(tokens);
      continue;
    }

    cur = cur->next = stmt(tokens);
  }

  leave_scope();

  return new_block_node(start, head.next);
}

static Node* if_stmt(Token** tokens) {
  Token* start = *tokens;

  expect_tokens(tokens, 2, "if", "(");

  Node* node = new_node(ND_IF);
  node->token = start;

  node->cond = expr(tokens);

  expect_token(tokens, ")");

  node->then = stmt(tokens);

  if (consume_token(tokens, "else")) {
    node->els = stmt(tokens);
  }

  return node;
}

static Node* switch_stmt(Token** tokens) {
  Token* start = *tokens;

  expect_tokens(tokens, 2, "switch", "(");

  Node* prev_switch = current_switch;
  Node* node = current_switch = new_node(ND_SWITCH);
  node->token = start;

  char* prev_break_label_id = renew_break_label_id(&node->break_label_id);

  node->cond = expr(tokens);

  expect_token(tokens, ")");

  node->then = stmt(tokens);

  current_switch = prev_switch;
  current_break_label_id = prev_break_label_id;

  return node;
}

static Node* case_stmt(Token** tokens) {
  Token* start = *tokens;

  expect_token(tokens, "case");

  if (!current_switch) {
    error_token(start, "stray case");
  }

  Node* node = new_node(ND_CASE);
  node->token = start;
  node->label_id = new_id();

  node->int_val = const_expr(tokens);
  expect_token(tokens, ":");

  node->lhs = stmt(tokens);

  node->cases = current_switch->cases;
  current_switch->cases = node;

  return node;
}

static Node* default_case_stmt(Token** tokens) {
  Token* start = *tokens;

  expect_token(tokens, "default");
  expect_token(tokens, ":");

  if (!current_switch) {
    error_token(start, "stray default");
  }

  Node* node = new_node(ND_CASE);
  node->token = start;
  node->label_id = current_switch->default_label_id = new_id();

  node->lhs = stmt(tokens);

  return node;
}

static Node* for_stmt(Token** tokens) {
  Token* start = *tokens;

  expect_tokens(tokens, 2, "for", "(");

  Node* node = new_node(ND_FOR);
  node->token = start;

  enter_scope();

  char* prev_break_label_id = renew_break_label_id(&node->break_label_id);
  char* prev_continue_label_id = renew_continue_label_id(&node->continue_label_id);

  if (!consume_token(tokens, ";")) {
    if (equal_to_decl_specifier(*tokens)) {
      node->init = lvar_decl(tokens);
    } else {
      node->init = expr(tokens);
    }
    expect_token(tokens, ";");
  }

  if (!consume_token(tokens, ";")) {
    node->cond = expr(tokens);
    expect_token(tokens, ";");
  }

  if (!consume_token(tokens, ")")) {
    node->inc = expr(tokens);
    expect_token(tokens, ")");
  }

  node->then = stmt(tokens);

  leave_scope();

  current_break_label_id = prev_break_label_id;
  current_continue_label_id = prev_continue_label_id;

  return node;
}

static Node* while_stmt(Token** tokens) {
  Token* start = *tokens;

  expect_tokens(tokens, 2, "while", "(");

  Node* node = new_node(ND_FOR);
  node->token = start;

  char* prev_break_label_id = renew_break_label_id(&node->break_label_id);
  char* prev_continue_label_id = renew_continue_label_id(&node->continue_label_id);

  node->cond = expr(tokens);

  expect_token(tokens, ")");

  node->then = stmt(tokens);

  current_break_label_id = prev_break_label_id;
  current_continue_label_id = prev_continue_label_id;

  return node;
}

static Node* do_stmt(Token** tokens) {
  Token* start = *tokens;

  expect_token(tokens, "do");

  Node* node = new_node(ND_FOR);
  node->token = start;

  char* prev_break_label_id = renew_break_label_id(&node->break_label_id);
  char* prev_continue_label_id = renew_continue_label_id(&node->continue_label_id);

  node->then = stmt(tokens);

  current_break_label_id = prev_break_label_id;
  current_continue_label_id = prev_continue_label_id;

  expect_tokens(tokens, 2, "while", "(");

  node->cond = expr(tokens);

  expect_tokens(tokens, 2, ")", ";");

  Node* first = calloc(1, sizeof(Node));
  *first = *node->then;
  first->next = node;

  return new_block_node(start, first);
}

static Node* return_stmt(Token** tokens) {
  Token* start = *tokens;

  expect_token(tokens, "return");

  if (consume_token(tokens, ";")) {
    Node* node = new_node(ND_RETURN);
    node->token = start;
    return node;
  }

  Node* lhs = expr(tokens);
  if (!is_composite_type(lhs->type)) {
    lhs = new_cast_node(current_func->type->return_type, *tokens, lhs);
  }
  expect_token(tokens, ";");

  Node* node = new_unary_node(ND_RETURN, lhs);
  node->token = start;

  return node;
}

static Node* label_stmt(Token** tokens) {
  Token* start = *tokens;

  Token* ident = expect_ident_token(tokens);
  expect_token(tokens, ":");

  return create_label_node(start, strndup(ident->loc, ident->len), stmt(tokens));
}

static Node* goto_stmt(Token** tokens) {
  Token* start = *tokens;

  expect_token(tokens, "goto");

  Token* ident = expect_ident_token(tokens);
  expect_token(tokens, ";");

  return create_goto_node(start, strndup(ident->loc, ident->len));
}

static Node* break_stmt(Token** tokens) {
  Token* start = *tokens;

  expect_token(tokens, "break");

  if (!current_break_label_id) {
    error_token(start, "stray break");
  }

  return new_contextual_goto_node(start, current_break_label_id);
}

static Node* continue_stmt(Token** tokens) {
  Token* start = *tokens;

  expect_token(tokens, "continue");

  if (!current_continue_label_id) {
    error_token(start, "stray continue");
  }

  return new_contextual_goto_node(start, current_continue_label_id);
}

static Node* stmt(Token** tokens) {
  if (equal_to_token(*tokens, "{")) {
    return block_stmt(tokens);
  }

  if (equal_to_token(*tokens, "if")) {
    return if_stmt(tokens);
  }

  if (equal_to_token(*tokens, "switch")) {
    return switch_stmt(tokens);
  }

  if (equal_to_token(*tokens, "case")) {
    return case_stmt(tokens);
  }

  if (equal_to_token(*tokens, "default")) {
    return default_case_stmt(tokens);
  }

  if (equal_to_token(*tokens, "for")) {
    return for_stmt(tokens);
  }

  if (equal_to_token(*tokens, "while")) {
    return while_stmt(tokens);
  }

  if (equal_to_token(*tokens, "do")) {
    return do_stmt(tokens);
  }

  if (equal_to_token(*tokens, "return")) {
    return return_stmt(tokens);
  }

  if ((*tokens)->kind == TK_IDENT && equal_to_token((*tokens)->next, ":")) {
    return label_stmt(tokens);
  }

  if (equal_to_token(*tokens, "goto")) {
    return goto_stmt(tokens);
  }

  if (equal_to_token(*tokens, "break")) {
    return break_stmt(tokens);
  }

  if (equal_to_token(*tokens, "continue")) {
    return continue_stmt(tokens);
  }

  if (equal_to_decl_specifier(*tokens)) {
    return lvar(tokens);
  }

  return expr_stmt(tokens);
}

static Node* expr_stmt(Token** tokens) {
  Token* start = *tokens;

  if (consume_token(tokens, ";")) {
    return new_block_node(start, NULL);
  }

  Node* node = new_expr_stmt_node(expr(tokens));
  expect_token(tokens, ";");
  return node;
}

static Node* expr(Token** tokens) {
  Token* start = *tokens;

  Node* node = assign(tokens);

  if (consume_token(tokens, ",")) {
    return new_comma_node(start, node, expr(tokens));
  }

  return node;
}

static Node* convert_to_assign_node(Token* token, NodeKind op, Node* lhs, Node* rhs) {
  Node* tmp_assign = NULL;
  Node* assign = NULL;

  // When the assignee is the member of a composite type,
  // convert A.x op= B to tmp = &A, (*tmp).x = (*tmp).x op B
  // as it is not possible to reference the member if it is bitfield.
  if (lhs->kind == ND_MEMBER) {
    Obj* tmp_var = create_anon_lvar_obj(new_ptr_type(lhs->lhs->type));
    tmp_assign = new_assign_node(lhs->token, new_var_node(lhs->token, tmp_var), new_addr_node(lhs->token, lhs->lhs));

    Node* tmp_member
      = new_member_node(lhs->token, new_deref_node(lhs->token, new_var_node(lhs->token, tmp_var)), lhs->mem);

    assign = new_shorthand_assign_node(lhs->token, op, tmp_member, rhs);
  } else {
    // Otherwise, convert A op= B to tmp = &A, *tmp = *tmp op B.
    Obj* tmp_var = create_anon_lvar_obj(new_ptr_type(lhs->type));
    tmp_assign = new_assign_node(lhs->token, new_var_node(lhs->token, tmp_var), new_addr_node(lhs->token, lhs));

    Node* tmp_deref = new_deref_node(lhs->token, new_var_node(lhs->token, tmp_var));

    assign = new_shorthand_assign_node(lhs->token, op, tmp_deref, rhs);
  }

  return new_comma_node(token, tmp_assign, assign);
}

static Node* assign(Token** tokens) {
  Node* node = conditional(tokens);

  for (;;) {
    Token* start = *tokens;

    if (consume_token(tokens, "=")) {
      node = new_assign_node(start, node, assign(tokens));
      continue;
    }

    if (consume_token(tokens, "|=")) {
      node = convert_to_assign_node(start, ND_BITOR, node, assign(tokens));
      continue;
    }

    if (consume_token(tokens, "^=")) {
      node = convert_to_assign_node(start, ND_BITXOR, node, assign(tokens));
      continue;
    }

    if (consume_token(tokens, "&=")) {
      node = convert_to_assign_node(start, ND_BITAND, node, assign(tokens));
      continue;
    }

    if (consume_token(tokens, "<<=")) {
      node = convert_to_assign_node(start, ND_LSHIFT, node, assign(tokens));
      continue;
    }

    if (consume_token(tokens, ">>=")) {
      node = convert_to_assign_node(start, ND_RSHIFT, node, assign(tokens));
      continue;
    }

    if (consume_token(tokens, "+=")) {
      node = convert_to_assign_node(start, ND_ADD, node, assign(tokens));
      continue;
    }

    if (consume_token(tokens, "-=")) {
      node = convert_to_assign_node(start, ND_SUB, node, assign(tokens));
      continue;
    }

    if (consume_token(tokens, "*=")) {
      node = convert_to_assign_node(start, ND_MUL, node, assign(tokens));
      continue;
    }

    if (consume_token(tokens, "/=")) {
      node = convert_to_assign_node(start, ND_DIV, node, assign(tokens));
      continue;
    }

    if (consume_token(tokens, "%=")) {
      node = convert_to_assign_node(start, ND_MOD, node, assign(tokens));
      continue;
    }

    return node;
  }
}

static Node* conditional(Token** tokens) {
  Node* node = log_or(tokens);

  if (consume_token(tokens, "?")) {
    Token* start = *tokens;

    Node* then = expr(tokens);
    expect_token(tokens, ":");
    Node* els = conditional(tokens);

    return new_cond_node(start, node, then, els);
  }

  return node;
}

static Node* log_or(Token** tokens) {
  Node* node = log_and(tokens);

  while (consume_token(tokens, "||")) {
    Token* start = *tokens;
    node = new_or_node(start, node, log_and(tokens));
  }

  return node;
}

static Node* log_and(Token** tokens) {
  Node* node = bit_or(tokens);

  while (consume_token(tokens, "&&")) {
    Token* start = *tokens;
    node = new_and_node(start, node, bit_or(tokens));
  }

  return node;
}

static Node* bit_or(Token** tokens) {
  Node* node = bit_xor(tokens);

  while (consume_token(tokens, "|")) {
    Token* start = *tokens;
    node = new_bitor_node(start, node, bit_xor(tokens));
  }

  return node;
}

static Node* bit_xor(Token** tokens) {
  Node* node = bit_and(tokens);

  while (consume_token(tokens, "^")) {
    Token* start = *tokens;
    node = new_bitxor_node(start, node, bit_and(tokens));
  }

  return node;
}

static Node* bit_and(Token** tokens) {
  Node* node = equality(tokens);

  while (consume_token(tokens, "&")) {
    Token* start = *tokens;
    node = new_bitand_node(start, node, equality(tokens));
  }

  return node;
}

static Node* equality(Token** tokens) {
  Node* node = relational(tokens);

  for (;;) {
    Token* start = *tokens;

    if (consume_token(tokens, "==")) {
      node = new_eq_node(start, node, relational(tokens));
      continue;
    }

    if (consume_token(tokens, "!=")) {
      node = new_ne_node(start, node, relational(tokens));
      continue;
    }

    return node;
  }
}

static Node* relational(Token** tokens) {
  Node* node = shift(tokens);

  for (;;) {
    Token* start = *tokens;

    if (consume_token(tokens, "<")) {
      node = new_lt_node(start, node, shift(tokens));
      continue;
    }

    if (consume_token(tokens, "<=")) {
      node = new_le_node(start, node, shift(tokens));
      continue;
    }

    if (consume_token(tokens, ">")) {
      node = new_lt_node(start, shift(tokens), node);
      continue;
    }

    if (consume_token(tokens, ">=")) {
      node = new_le_node(start, shift(tokens), node);
      continue;
    }

    return node;
  }
}

static Node* shift(Token** tokens) {
  Node* node = add(tokens);

  for (;;) {
    Token* start = *tokens;

    if (consume_token(tokens, "<<")) {
      node = new_lshift_node(start, node, add(tokens));
      continue;
    }

    if (consume_token(tokens, ">>")) {
      node = new_rshift_node(start, node, add(tokens));
      continue;
    }

    return node;
  }
}

static Node* add(Token** tokens) {
  Node* node = mul(tokens);

  for (;;) {
    Token* start = *tokens;

    if (consume_token(tokens, "+")) {
      node = new_add_node(start, node, mul(tokens));
      continue;
    }

    if (consume_token(tokens, "-")) {
      node = new_sub_node(start, node, mul(tokens));
      continue;
    }

    return node;
  }
}

static Node* mul(Token** tokens) {
  Node* node = cast(tokens);

  for (;;) {
    Token* start = *tokens;

    if (consume_token(tokens, "*")) {
      node = new_mul_node(start, node, cast(tokens));
      continue;
    }

    if (consume_token(tokens, "/")) {
      node = new_div_node(start, node, cast(tokens));
      continue;
    }

    if (consume_token(tokens, "%")) {
      node = new_mod_node(start, node, cast(tokens));
      continue;
    }

    return node;
  }
}

static Node* cast(Token** tokens) {
  if (equal_to_abstract_decl_start(*tokens)) {
    Token* start = *tokens;
    expect_token(tokens, "(");
    Type* type = abstract_decl(tokens, NULL);
    expect_token(tokens, ")");

    // compound literals
    if (equal_to_token(*tokens, "{")) {
      *tokens = start;
      return unary(tokens);
    }

    return new_cast_node(type, start, cast(tokens));
  }

  return unary(tokens);
}

static Node* unary(Token** tokens) {
  Token* start = *tokens;

  if (consume_token(tokens, "+")) {
    return cast(tokens);
  }

  if (consume_token(tokens, "-")) {
    return new_neg_node(start, cast(tokens));
  }

  if (consume_token(tokens, "&")) {
    return new_addr_node(start, cast(tokens));
  }

  if (consume_token(tokens, "*")) {
    Node* lhs = cast(tokens);
    // According to C spec, dereferencing function pointer should do nothing.
    return lhs->kind != ND_FUNC ? new_deref_node(start, lhs) : lhs;
  }

  if (consume_token(tokens, "!")) {
    return new_not_node(start, cast(tokens));
  }

  if (consume_token(tokens, "~")) {
    return new_bitnot_node(start, cast(tokens));
  }

  if (consume_token(tokens, "++")) {
    return convert_to_assign_node(start, ND_ADD, unary(tokens), new_int_node(start, 1));
  }

  if (consume_token(tokens, "--")) {
    return convert_to_assign_node(start, ND_SUB, unary(tokens), new_int_node(start, 1));
  }

  if (equal_to_token(*tokens, "sizeof") && equal_to_abstract_decl_start((*tokens)->next)) {
    expect_tokens(tokens, 2, "sizeof", "(");
    Type* type = abstract_decl(tokens, NULL);
    expect_token(tokens, ")");
    return new_ulong_node(start, type->size);
  }

  if (consume_token(tokens, "sizeof")) {
    Node* node = unary(tokens);
    return new_ulong_node(start, node->type->size);
  }

  if (equal_to_token(*tokens, "_Alignof") && equal_to_abstract_decl_start((*tokens)->next)) {
    expect_tokens(tokens, 2, "_Alignof", "(");
    Type* type = abstract_decl(tokens, NULL);
    expect_token(tokens, ")");
    return new_ulong_node(start, type->alignment);
  }

  if (consume_token(tokens, "_Alignof")) {
    Node* node = unary(tokens);
    return new_ulong_node(start, node->type->size);
  }

  if (consume_token(tokens, "__builtin_reg_class")) {
    expect_token(tokens, "(");
    Type* type = abstract_decl(tokens, NULL);
    expect_token(tokens, ")");

    if (is_int_type(type) || type->kind == TY_PTR) {
      return new_int_node(start, 0);
    }
    if (is_float_type(type)) {
      return new_int_node(start, 1);
    }
    return new_int_node(start, 2);
  }

  return postfix(tokens);
}

static Node* increment(Node* node, int delta) {
  Node* inc = convert_to_assign_node(node->token, ND_ADD, node, new_int_node(node->token, delta));
  return new_cast_node(node->type, node->token, new_add_node(node->token, inc, new_int_node(node->token, -delta)));
}

static Node* member(Token** tokens, Token* token, Node* lhs) {
  if (lhs->type->kind != TY_STRUCT && lhs->type->kind != TY_UNION) {
    error_token(*tokens, "expected a struct or union");
  }

  Token* ident = expect_ident_token(tokens);

  for (Member* mem = lhs->type->members; mem; mem = mem->next) {
    if (!equal_to_n_chars(mem->name, ident->loc, ident->len)) {
      continue;
    }

    return new_member_node(token, lhs, mem);
  }

  error_token(*tokens, "no such member");
  return NULL;
}

static Node* postfix(Token** tokens) {
  if (equal_to_abstract_decl_start(*tokens)) {
    return compound_literal(tokens);
  }

  Node* node = primary(tokens);

  for (;;) {
    Token* start = *tokens;

    if (equal_to_token(*tokens, "(")) {
      Type* type = deref_ptr_type(node->type);
      if (type->kind != TY_FUNC) {
        error_token(node->token, "not a function");
      }
      node = new_funccall_node(start, node, func_args(tokens, type));
      continue;
    }

    if (consume_token(tokens, "[")) {
      Node* index = expr(tokens);
      expect_token(tokens, "]");
      node = new_deref_node(start, new_add_node(*tokens, node, index));
      continue;
    }

    if (consume_token(tokens, ".")) {
      node = member(tokens, start, node);
      continue;
    }

    if (consume_token(tokens, "->")) {
      node = new_deref_node(start, node);
      node = member(tokens, start, node);
      continue;
    }

    if (consume_token(tokens, "++")) {
      node = increment(node, 1);
      continue;
    }

    if (consume_token(tokens, "--")) {
      node = increment(node, -1);
      continue;
    }

    return node;
  }
}

static Node* compound_literal(Token** tokens) {
  expect_token(tokens, "(");

  VarAttr attr = {};
  Type* type = abstract_decl(tokens, &attr);

  expect_token(tokens, ")");

  if (is_gscope(current_scope)) {
    Obj* var = create_anon_gvar_obj(type);
    gvar_initer(tokens, var);
    return new_var_node(*tokens, var);
  }

  Obj* var = create_anon_lvar_obj(type);
  Node* assign = lvar_initer(tokens, var);
  return new_comma_node(*tokens, assign, new_var_node(*tokens, var));
}

static Node* datum(Token** tokens) {
  Token* ident = expect_ident_token(tokens);
  Obj* datum = find_datum_obj(ident->loc, ident->len);
  if (!datum) {
    if (equal_to_token(*tokens, "(")) {
      error_token(ident, "implicit declaration of a function");
    } else {
      error_token(ident, "undefined variable");
    }
  }

  switch (datum->kind) {
    case OJ_FUNC:
      return new_func_node(datum->type, ident, datum->name, datum->is_definition);
    case OJ_GVAR:
    case OJ_LVAR:
      return new_var_node(ident, datum);
    case OJ_ENUM:
      return new_int_node(ident, (int64_t)datum->val);
    default:
      return NULL;
  }
}

static Node* primary(Token** tokens) {
  Token* start = *tokens;

  if (equal_to_tokens(*tokens, 2, "(", "{")) {
    expect_token(tokens, "(");
    Node* node = new_stmt_expr_node(start, block_stmt(tokens)->body);
    expect_token(tokens, ")");
    return node;
  }

  if (consume_token(tokens, "(")) {
    Node* node = expr(tokens);
    expect_token(tokens, ")");
    return node;
  }

  if ((*tokens)->kind == TK_IDENT) {
    return datum(tokens);
  }

  if ((*tokens)->kind == TK_NUM) {
    Node* node = is_float_type((*tokens)->type) ? new_float_node(*tokens, (*tokens)->float_val)
                                                : new_int_node(*tokens, (*tokens)->int_val);
    *tokens = (*tokens)->next;
    return node;
  }

  if ((*tokens)->kind == TK_STR) {
    Obj* str = create_str_obj((*tokens)->str_val, (*tokens)->str_val_len);
    Node* node = new_var_node(*tokens, str);
    *tokens = (*tokens)->next;
    return node;
  }

  error_token(*tokens, "expected a primary");
  return NULL;
}

static int64_t relocate(Node* node, char** label) {
  switch (node->kind) {
    case ND_LVAR:
      error_token(node->token, "not a compile-time constant");
      return 0;
    case ND_GVAR:
      *label = node->name;
      return 0;
    case ND_DEREF:
      return eval_reloc(node->lhs, label);
    case ND_MEMBER:
      return relocate(node->lhs, label) + node->mem->offset;
    default:
      error_token(node->token, "invalid initializer");
      return 0;
  }
}

static int64_t eval(Node* node) {
  return eval_reloc(node, NULL);
}

static double eval_float(Node* node) {
  if (is_int_type(node->type)) {
    return node->type->is_unsigned ? (unsigned long)eval(node) : eval(node);
  }

  switch (node->kind) {
    case ND_COMMA:
      return eval_float(node->rhs);
    case ND_COND:
      return eval_float(node->cond) ? eval_float(node->then) : eval_float(node->els);
    case ND_ADD:
      return eval_float(node->lhs) + eval_float(node->rhs);
    case ND_SUB:
      return eval_float(node->lhs) - eval_float(node->rhs);
    case ND_MUL:
      return eval_float(node->lhs) * eval_float(node->rhs);
    case ND_DIV:
      return eval_float(node->lhs) / eval_float(node->rhs);
    case ND_CAST:
      return is_float_type(node->lhs->type) ? eval_float(node->lhs) : eval(node->lhs);
    case ND_NEG:
      return -eval_float(node->lhs);
    case ND_NUM:
      return node->float_val;
    default:
      error_token(node->token, "not a compile-time constant");
      return 0;
  }
}

static int64_t eval_reloc(Node* node, char** label) {
  if (is_float_type(node->type)) {
    return eval_float(node);
  }

  switch (node->kind) {
    case ND_COMMA:
      return eval_reloc(node->rhs, label);
    case ND_COND:
      return eval(node->cond) ? eval_reloc(node->then, label) : eval_reloc(node->els, label);
    case ND_LOGOR:
      return eval(node->lhs) || eval(node->rhs);
    case ND_LOGAND:
      return eval(node->lhs) && eval(node->rhs);
    case ND_BITOR:
      return eval(node->lhs) | eval(node->rhs);
    case ND_BITXOR:
      return eval(node->lhs) ^ eval(node->rhs);
    case ND_BITAND:
      return eval(node->lhs) & eval(node->rhs);
    case ND_EQ:
      return eval(node->lhs) == eval(node->rhs);
    case ND_NE:
      return eval(node->lhs) != eval(node->rhs);
    case ND_LT:
      if (node->lhs->type->is_unsigned) {
        return (uint64_t)eval(node->lhs) < eval(node->rhs);
      }
      return eval(node->lhs) < eval(node->rhs);
    case ND_LE:
      if (node->lhs->type->is_unsigned) {
        return (uint64_t)eval(node->lhs) <= eval(node->rhs);
      }
      return eval(node->lhs) <= eval(node->rhs);
    case ND_LSHIFT:
      return eval(node->lhs) << eval(node->rhs);
    case ND_RSHIFT:
      if (node->type->is_unsigned && node->type->size == 8) {
        return (uint64_t)eval(node->lhs) >> eval(node->rhs);
      }
      return eval(node->lhs) >> eval(node->rhs);
    case ND_ADD:
      return eval_reloc(node->lhs, label) + eval(node->rhs);
    case ND_SUB:
      return eval_reloc(node->lhs, label) - eval(node->rhs);
    case ND_MUL:
      return eval(node->lhs) * eval(node->rhs);
    case ND_DIV:
      if (node->type->is_unsigned) {
        return (uint64_t)eval(node->lhs) / eval(node->rhs);
      }
      return eval(node->lhs) / eval(node->rhs);
    case ND_MOD:
      if (node->type->is_unsigned) {
        return (uint64_t)eval(node->lhs) % eval(node->rhs);
      }
      return eval(node->lhs) % eval(node->rhs);
    case ND_CAST: {
      int64_t val = eval_reloc(node->lhs, label);
      if (!is_int_type(node->type)) {
        return val;
      }
      switch (node->type->size) {
        case 1:
          return node->type->is_unsigned ? (uint8_t)val : (int8_t)val;
        case 2:
          return node->type->is_unsigned ? (uint16_t)val : (int16_t)val;
        case 4:
          return node->type->is_unsigned ? (uint32_t)val : (int32_t)val;
        default:
          return val;
      }
    }
    case ND_NEG:
      return -eval(node->lhs);
    case ND_ADDR:
      return relocate(node->lhs, label);
    case ND_NOT:
      return !eval(node->lhs);
    case ND_BITNOT:
      return ~eval(node->lhs);
    case ND_LVAR:
    case ND_GVAR:
      if (!label) {
        error_token(node->token, "not a compile-time constant");
      }
      if (node->type->kind != TY_ARRAY) {
        error_token(node->token, "invalid initializer");
      }

      *label = node->name;
      return 0;
    case ND_MEMBER:
      if (!label) {
        error_token(node->token, "not a compile-time constant");
      }
      if (node->type->kind != TY_ARRAY) {
        error_token(node->token, "invalid initializer");
      }

      return relocate(node->lhs, label) + node->mem->offset;
    case ND_NUM:
      return node->int_val;
    default:
      error_token(node->token, "not a compile-time constant");
      return 0;
  }
}

int64_t const_expr(Token** tokens) {
  Node* node = conditional(tokens);
  return eval(node);
}

static Node* lvar(Token** tokens) {
  Node* node = lvar_decl(tokens);
  expect_token(tokens, ";");
  return node;
}

static Initer* new_initer(Type* type) {
  Initer* init = calloc(1, sizeof(Initer));
  init->type = type;

  if (is_composite_type(init->type)) {
    int mems = 0;
    for (Member* mem = init->type->members; mem; mem = mem->next) {
      mems++;
    }

    init->children = calloc(mems, sizeof(Initer*));
    int i = 0;
    for (Member* mem = init->type->members; mem; mem = mem->next) {
      init->children[i] = new_initer(mem->type);
      if (!mem->next && type->is_flexible) {
        init->children[i]->is_flexible = true;
      }

      i++;
    }
    return init;
  }

  if (init->type->kind == TY_ARRAY) {
    if (init->type->size < 0) {
      init->is_flexible = true;
      return init;
    }

    init->children = calloc(init->type->size, sizeof(Initer*));
    for (int i = 0; i < init->type->size; i++) {
      init->children[i] = new_initer(init->type->base);
    }
    return init;
  }

  return init;
}

static void skip_excess_initers(Token** tokens) {
  if (consume_token(tokens, "{")) {
    skip_excess_initers(tokens);
    expect_token(tokens, "}");
    return;
  }

  expr(tokens);
}

static bool equal_to_initer_end(Token* token) {
  return equal_to_token(token, "}") || equal_to_tokens(token, 2, ",", "}");
}

static bool consume_initer_end(Token** tokens) {
  if (consume_token(tokens, "}")) {
    return true;
  }

  if (equal_to_tokens(*tokens, 2, ",", "}")) {
    expect_tokens(tokens, 2, ",", "}");
    return true;
  }

  return false;
}

static void expect_initer_end(Token** tokens) {
  if (!consume_initer_end(tokens)) {
    error_token(*tokens, "expected an end of initialier");
  }
}

static void init_initer(Token** tokens, Initer* init);

static void init_string_initer(Token** tokens, Initer* init) {
  int len = strlen((*tokens)->str_val) + 1;

  if (init->is_flexible) {
    *init = *new_initer(new_array_type(init->type->base, len));
  }

  if (init->type->len < len) {
    len = init->type->len;
  }

  for (int i = 0; i < len; i++) {
    init->children[i]->expr = new_int_node(*tokens, (*tokens)->str_val[i]);
  }
  *tokens = (*tokens)->next;
}

static void init_struct_initer(Token** tokens, Initer* init) {
  expect_token(tokens, "{");

  int i = 0;
  Member* mem = init->type->members;
  while (!consume_initer_end(tokens)) {
    if (i > 0) {
      expect_token(tokens, ",");
    }

    if (mem) {
      init_initer(tokens, init->children[i]);
      i++;

      mem = mem->next;
      continue;
    }

    skip_excess_initers(tokens);
  }
}

static void init_direct_struct_initer(Token** tokens, Initer* init) {
  int i = 0;
  for (Member* mem = init->type->members; mem && !equal_to_initer_end(*tokens); mem = mem->next) {
    if (i > 0) {
      expect_token(tokens, ",");
    }

    init_initer(tokens, init->children[i]);
    i++;
  }
}

static void init_union_initer(Token** tokens, Initer* init) {
  if (consume_token(tokens, "{")) {
    init_initer(tokens, init->children[0]);
    expect_initer_end(tokens);
  } else {
    init_initer(tokens, init->children[0]);
  }
}

static int count_initers(Token* token, Type* type) {
  Initer* ignored = new_initer(type);

  int i = 0;
  for (; !consume_initer_end(&token); i++) {
    if (i > 0) {
      expect_token(&token, ",");
    }
    init_initer(&token, ignored);
  }
  return i;
}

static void init_array_initer(Token** tokens, Initer* init) {
  expect_token(tokens, "{");

  if (init->is_flexible) {
    int len = count_initers(*tokens, init->type->base);
    *init = *new_initer(new_array_type(init->type->base, len));
    init->is_flexible = true;
  }

  for (int i = 0; !consume_initer_end(tokens); i++) {
    if (i > 0) {
      expect_token(tokens, ",");
    }

    if (i < init->type->len) {
      init_initer(tokens, init->children[i]);
      continue;
    }

    skip_excess_initers(tokens);
  }
}

static void init_direct_array_initer(Token** tokens, Initer* init) {
  if (init->is_flexible) {
    int len = count_initers(*tokens, init->type->base);
    *init = *new_initer(new_array_type(init->type->base, len));
    init->is_flexible = true;
  }

  for (int i = 0; i < init->type->len && !equal_to_initer_end(*tokens); i++) {
    if (i > 0) {
      expect_token(tokens, ",");
    }
    init_initer(tokens, init->children[i]);
  }
}

static void init_initer(Token** tokens, Initer* init) {
  if (init->type->kind == TY_ARRAY && (*tokens)->kind == TK_STR) {
    init_string_initer(tokens, init);
    return;
  }

  if (init->type->kind == TY_STRUCT) {
    if (equal_to_token(*tokens, "{")) {
      init_struct_initer(tokens, init);
      return;
    }

    Token* start = *tokens;

    Node* expr = assign(tokens);
    if (expr->type->kind == TY_STRUCT) {
      init->expr = expr;
      return;
    }

    *tokens = start;

    init_direct_struct_initer(tokens, init);
    return;
  }

  if (init->type->kind == TY_UNION) {
    init_union_initer(tokens, init);
    return;
  }

  if (init->type->kind == TY_ARRAY) {
    if (equal_to_token(*tokens, "{")) {
      init_array_initer(tokens, init);
    } else {
      init_direct_array_initer(tokens, init);
    }
    return;
  }

  if (consume_token(tokens, "{")) {
    init->expr = assign(tokens);
    expect_token(tokens, "}");
    return;
  }

  init->expr = assign(tokens);
}

static Initer* initer(Token** tokens, Type** type) {
  Initer* init = new_initer(*type);
  init_initer(tokens, init);

  if (init->type->kind == TY_STRUCT && init->type->is_flexible) {
    Type* copied = copy_composite_type(*type, init->type->kind);

    int i = 0;
    Member* mem = copied->members;
    while (mem->next) {
      i++;
      mem = mem->next;
    }
    mem->type = init->children[i]->type;
    copied->size += mem->type->size;

    *type = copied;
    return init;
  }

  *type = init->type;
  return init;
}

static Node* designated_expr(Token* token, DesignatedIniter* init) {
  if (init->var) {
    return new_var_node(token, init->var);
  }

  if (init->mem) {
    return new_member_node(token, designated_expr(token, init->next), init->mem);
  }

  Node* lhs = designated_expr(token, init->next);
  Node* rhs = new_int_node(token, init->index);
  return new_deref_node(token, new_add_node(token, lhs, rhs));
}

static Node* lvar_init(Token* token, Initer* init, DesignatedIniter* designated) {
  if (init->type->kind == TY_STRUCT && !init->expr) {
    Node* node = new_null_node(token);

    int i = 0;
    for (Member* mem = init->type->members; mem; mem = mem->next) {
      DesignatedIniter next = {designated, 0, NULL, mem};
      node = new_comma_node(token, node, lvar_init(token, init->children[i], &next));
      i++;
    }

    return node;
  }

  if (init->type->kind == TY_UNION) {
    DesignatedIniter next = {designated, 0, NULL, init->type->members};
    return lvar_init(token, init->children[0], &next);
  }

  if (init->type->kind == TY_ARRAY) {
    Node* node = new_null_node(token);

    for (int i = 0; i < init->type->len; i++) {
      DesignatedIniter next = {designated, i, NULL};
      node = new_comma_node(token, node, lvar_init(token, init->children[i], &next));
    }

    return node;
  }

  return init->expr ? new_assign_node(token, designated_expr(token, designated), init->expr) : new_null_node(token);
}

static Node* lvar_initer(Token** tokens, Obj* var) {
  Token* start = *tokens;

  Type* type = var->type;
  Initer* init = initer(tokens, &type);
  // Adjust the offset of the var from the previous one (var->next)
  adjust_lvar_obj_offset(var->next, var, type);

  DesignatedIniter designated = {NULL, 0, var};
  return new_comma_node(start, new_memzero_node(start, var), lvar_init(start, init, &designated));
}

static Node* lvar_decl(Token** tokens) {
  Token* start = *tokens;

  VarAttr attr = {};
  Type* spec = decl_specifier(tokens, &attr);

  Node head = {};
  Node* cur = &head;
  while (!equal_to_token(*tokens, ";")) {
    if (cur != &head) {
      expect_token(tokens, ",");
    }

    Type* type = declarator(tokens, spec);
    if (!type->name) {
      error_token(type->ident, "variable name omitted");
    }
    if (type->kind == TY_VOID) {
      error_token(type->ident, "variable declared void");
    }

    if (attr.is_static) {
      Obj* var = create_static_lvar_obj(type, type->name);
      if (consume_token(tokens, "=")) {
        gvar_initer(tokens, var);
      }
      continue;
    }
    if (attr.alignment) {
      type = copy_type(type);
      type->alignment = attr.alignment;
    }

    Obj* var = create_lvar_obj(type, type->name);
    Node* node = new_var_node(type->ident, var);

    if (consume_token(tokens, "=")) {
      node = lvar_initer(tokens, var);
    }
    if (var->type->size < 0) {
      error_token(type->ident, "variable has imcomplete type");
    }

    cur = cur->next = node;
  }

  return new_block_node(start, head.next);
}

static Type* enum_specifier(Token** tokens) {
  expect_token(tokens, "enum");

  Token* tag = NULL;
  if ((*tokens)->kind == TK_IDENT) {
    tag = expect_ident_token(tokens);
  }

  if (tag && !equal_to_token(*tokens, "{")) {
    Obj* found = find_tag_obj(tag->loc, tag->len);
    if (!found) {
      error_token(tag, "undefined tag");
    }
    return found->type;
  }

  expect_token(tokens, "{");

  bool is_first = true;
  int val = 0;
  while (!consume_initer_end(tokens)) {
    if (!is_first) {
      expect_token(tokens, ",");
    }
    is_first = false;

    Token* ident = expect_ident_token(tokens);

    if (consume_token(tokens, "=")) {
      val = const_expr(tokens);
    }

    create_enum_obj(strndup(ident->loc, ident->len), val++);
  }

  Type* enu = new_int_type();

  if (tag) {
    create_tag_obj(enu, strndup(tag->loc, tag->len));
  }

  return enu;
}

static Type* struct_decl(Token** tokens) {
  expect_token(tokens, "struct");

  Token* tag = NULL;
  if ((*tokens)->kind == TK_IDENT) {
    tag = expect_ident_token(tokens);
  }

  if (tag && !equal_to_token(*tokens, "{")) {
    Obj* found = find_tag_obj(tag->loc, tag->len);
    if (found) {
      return found->type;
    }

    Type* struc = new_struct_type(-1, 1, NULL);
    create_tag_obj(struc, strndup(tag->loc, tag->len));
    return struc;
  }

  expect_token(tokens, "{");

  bool is_flexible = false;
  Member* mems = members(tokens, &is_flexible);
  int bits = 0;
  int alignment = 1;
  for (Member* mem = mems; mem; mem = mem->next) {
    if (mem->type->kind == TY_VOID) {
      error_token(mem->token, "variable declared void");
    }
    if (mem->type->size < 0) {
      error_token(mem->token, "variable has imcomplete type");
    }

    if (mem->is_bitfield) {
      int size_in_bits = mem->type->size * 8;

      if (mem->bit_width == 0) {
        bits = align_up(bits, size_in_bits);
      } else {
        if (bits / size_in_bits != (bits + mem->bit_width - 1) / size_in_bits) {
          bits = align_up(bits, size_in_bits);
        }

        mem->offset = align_down(bits / 8, mem->type->size);
        mem->bit_offset = bits % size_in_bits;
        bits += mem->bit_width;
      }
    } else {
      bits = align_up(bits, mem->alignment * 8);
      mem->offset = bits / 8;
      bits += mem->type->size * 8;
    }

    if (alignment < mem->alignment) {
      alignment = mem->alignment;
    }
  }

  int size = align_up(bits, alignment * 8) / 8;
  Type* struc = new_struct_type(size, alignment, mems);
  struc->is_flexible = is_flexible;

  if (!tag) {
    return struc;
  }

  Obj* found = find_tag_obj_in_current_scope(tag->loc, tag->len);
  if (found) {
    *found->type = *struc;
    return found->type;
  }

  create_tag_obj(struc, strndup(tag->loc, tag->len));
  return struc;
}

static Type* union_decl(Token** tokens) {
  expect_token(tokens, "union");

  Token* tag = NULL;
  if ((*tokens)->kind == TK_IDENT) {
    tag = expect_ident_token(tokens);
  }

  if (tag && !equal_to_token(*tokens, "{")) {
    Obj* found = find_tag_obj(tag->loc, tag->len);
    if (found) {
      return found->type;
    }

    Type* uni = new_union_type(-1, 1, NULL);
    create_tag_obj(uni, strndup(tag->loc, tag->len));
    return uni;
  }

  expect_token(tokens, "{");

  bool is_flexible = false;
  Member* mems = members(tokens, &is_flexible);
  int size = 0;
  int alignment = 1;
  for (Member* mem = mems; mem; mem = mem->next) {
    if (mem->type->kind == TY_VOID) {
      error_token(mem->token, "variable declared void");
    }
    if (mem->type->size < 0) {
      error_token(mem->token, "variable has imcomplete type");
    }

    mem->offset = 0;
    if (size < mem->type->size) {
      size = mem->type->size;
    }
    if (alignment < mem->alignment) {
      alignment = mem->alignment;
    }
  }

  Type* uni = new_union_type(size, alignment, mems);

  if (!tag) {
    return uni;
  }

  Obj* found = find_tag_obj_in_current_scope(tag->loc, tag->len);
  if (found) {
    *found->type = *uni;
    return found->type;
  }

  create_tag_obj(uni, strndup(tag->loc, tag->len));
  return uni;
}

static Member* members(Token** tokens, bool* is_flexible) {
  Member head = {};
  Member* cur = &head;
  *is_flexible = false;
  while (!consume_token(tokens, "}")) {
    VarAttr attr = {};
    Type* spec = decl_specifier(tokens, &attr);

    bool is_first = true;
    while (!consume_token(tokens, ";")) {
      if (!is_first) {
        expect_token(tokens, ",");
      }
      is_first = false;

      Type* type = declarator(tokens, spec);

      Member* mem = new_member(type);
      if (attr.alignment) {
        mem->alignment = attr.alignment;
      }

      if (consume_token(tokens, ":")) {
        mem->is_bitfield = true;
        mem->bit_width = const_expr(tokens);
      } else if (!type->name) {
        // Members which are not bitfields cannot omit its name.
        error_token(type->ident, "member name omitted");
      }

      cur = cur->next = mem;
    }
  }

  if (cur != &head && cur->type->kind == TY_ARRAY && cur->type->size < 0) {
    cur->type = new_array_type(cur->type->base, 0);
    *is_flexible = true;
  }

  return head.next;
}

static Type* decl_specifier(Token** tokens, VarAttr* attr) {
  enum {
    VOID = 1 << 0,
    BOOL = 1 << 2,
    CHAR = 1 << 4,
    SHORT = 1 << 6,
    INT = 1 << 8,
    LONG = 1 << 10,
    FLOAT = 1 << 12,
    DOUBLE = 1 << 14,
    OTHER = 1 << 16,
    SIGNED = 1 << 17,
    UNSIGNED = 1 << 18,
  };

  Type* type = new_int_type();
  int counter = 0;

  while (equal_to_decl_specifier(*tokens)) {
    Token* start = *tokens;

    if (equal_to_token(*tokens, "typedef") || equal_to_token(*tokens, "extern") || equal_to_token(*tokens, "static")) {
      if (!attr) {
        error_token(*tokens, "storage class specifier is not allowed in this context");
      }

      attr->is_typedef = equal_to_token(*tokens, "typedef");
      attr->is_extern = equal_to_token(*tokens, "extern");
      attr->is_static = equal_to_token(*tokens, "static");

      if (attr->is_typedef && (attr->is_extern || attr->is_static)) {
        error_token(*tokens, "typedef may not be used with extern or static");
      }

      *tokens = (*tokens)->next;
      continue;
    }

    // Ignore those types for now
    if (consume_token(tokens, "const") || consume_token(tokens, "volatile") || consume_token(tokens, "auto")
        || consume_token(tokens, "register") || consume_token(tokens, "restrict") || consume_token(tokens, "__restrict")
        || consume_token(tokens, "__restrict__") || consume_token(tokens, "_Noreturn")) {
      continue;
    }

    if (equal_to_token(*tokens, "_Alignas")) {
      if (!attr) {
        error_token(*tokens, "_Alignas is not allowed in this context");
      }

      expect_tokens(tokens, 2, "_Alignas", "(");
      if (equal_to_decl_specifier(*tokens)) {
        Type* type = abstract_decl(tokens, NULL);
        attr->alignment = type->alignment;
      } else {
        attr->alignment = const_expr(tokens);
      }
      expect_token(tokens, ")");
      continue;
    }

    Obj* def_type = find_def_type_obj((*tokens)->loc, (*tokens)->len);
    if (def_type) {
      if (counter > 0) {
        break;
      }

      counter += OTHER;
      type = def_type->type;
      *tokens = (*tokens)->next;
      continue;
    }

    if (equal_to_token(*tokens, "struct")) {
      if (counter > 0) {
        break;
      }

      counter += OTHER;
      type = struct_decl(tokens);
      continue;
    }

    if (equal_to_token(*tokens, "union")) {
      if (counter > 0) {
        break;
      }

      counter += OTHER;
      type = union_decl(tokens);
      continue;
    }

    if (equal_to_token(*tokens, "enum")) {
      if (counter > 0) {
        break;
      }

      counter += OTHER;
      type = enum_specifier(tokens);
      continue;
    }

    if (consume_token(tokens, "void")) {
      counter += VOID;
    }

    if (consume_token(tokens, "_Bool")) {
      counter += BOOL;
    }

    if (consume_token(tokens, "char")) {
      counter += CHAR;
    }

    if (consume_token(tokens, "short")) {
      counter += SHORT;
    }

    if (consume_token(tokens, "int")) {
      counter += INT;
    }

    if (consume_token(tokens, "long")) {
      counter += LONG;
    }

    if (consume_token(tokens, "float")) {
      counter += FLOAT;
    }

    if (consume_token(tokens, "double")) {
      counter += DOUBLE;
    }

    if (consume_token(tokens, "signed")) {
      counter |= SIGNED;
    }

    if (consume_token(tokens, "unsigned")) {
      counter |= UNSIGNED;
    }

    switch (counter) {
      case VOID:
        type = new_void_type();
        break;
      case BOOL:
        type = new_bool_type();
        break;
      case CHAR:
      case SIGNED + CHAR:
        type = new_char_type();
        break;
      case UNSIGNED + CHAR:
        type = new_uchar_type();
        break;
      case SHORT:
      case SHORT + INT:
      case SIGNED + SHORT:
      case SIGNED + SHORT + INT:
        type = new_short_type();
        break;
      case UNSIGNED + SHORT:
      case UNSIGNED + SHORT + INT:
        type = new_ushort_type();
        break;
      case INT:
      case SIGNED:
      case SIGNED + INT:
        type = new_int_type();
        break;
      case UNSIGNED:
      case UNSIGNED + INT:
        type = new_uint_type();
        break;
      case LONG:
      case LONG + INT:
      case LONG + LONG:
      case LONG + LONG + INT:
      case SIGNED + LONG:
      case SIGNED + LONG + INT:
      case SIGNED + LONG + LONG:
      case SIGNED + LONG + LONG + INT:
        type = new_long_type();
        break;
      case UNSIGNED + LONG:
      case UNSIGNED + LONG + INT:
      case UNSIGNED + LONG + LONG:
      case UNSIGNED + LONG + LONG + INT:
        type = new_ulong_type();
        break;
      case FLOAT:
        type = new_float_type();
        break;
      case DOUBLE:
      case LONG + DOUBLE:
        type = new_double_type();
        break;
      default: {
        error_token(start, "expected a typename");
      }
    }
  }

  return type;
}

static Type* pointers(Token** tokens, Type* type) {
  while (consume_token(tokens, "*")) {
    type = new_ptr_type(type);

    while (consume_token(tokens, "const") || consume_token(tokens, "volatile") || consume_token(tokens, "restrict")
           || consume_token(tokens, "__restrict") || consume_token(tokens, "__restrict__")) {}
  }
  return type;
}

static Type* declarator(Token** tokens, Type* type) {
  type = pointers(tokens, type);

  if (consume_token(tokens, "(")) {
    Token* start = *tokens;
    Type ignored = {};
    declarator(tokens, &ignored);
    expect_token(tokens, ")");
    type = type_suffix(tokens, type);
    return declarator(&start, type);
  }

  Token* ident = (*tokens)->kind == TK_IDENT ? expect_ident_token(tokens) : *tokens;

  type = type_suffix(tokens, type);

  type->ident = ident;
  type->name = ident->kind == TK_IDENT ? strndup(ident->loc, ident->len) : NULL;
  return type;
}

static Type* decl(Token** tokens, VarAttr* attr) {
  return declarator(tokens, decl_specifier(tokens, attr));
}

static Type* abstract_declarator(Token** tokens, Type* type) {
  type = pointers(tokens, type);

  if (consume_token(tokens, "(")) {
    Token* start = *tokens;
    Type ignored = {};
    abstract_declarator(tokens, &ignored);
    expect_token(tokens, ")");
    type = type_suffix(tokens, type);
    return abstract_declarator(&start, type);
  }

  return type_suffix(tokens, type);
}

static Type* abstract_decl(Token** tokens, VarAttr* attr) {
  return abstract_declarator(tokens, decl_specifier(tokens, attr));
}

static Type* type_suffix(Token** tokens, Type* type) {
  if (equal_to_token(*tokens, "(")) {
    return func_params(tokens, type);
  }

  if (equal_to_token(*tokens, "[")) {
    return array_dimensions(tokens, type);
  }

  return type;
}

static Type* func_params(Token** tokens, Type* type) {
  expect_token(tokens, "(");

  if (equal_to_tokens(*tokens, 2, "void", ")")) {
    expect_tokens(tokens, 2, "void", ")");
    return new_func_type(type, NULL, false);
  }

  Type head = {};
  Type* cur = &head;
  bool is_variadic = false;
  while (!consume_token(tokens, ")")) {
    if (cur != &head) {
      expect_token(tokens, ",");
    }

    if (consume_token(tokens, "...")) {
      expect_token(tokens, ")");
      is_variadic = true;
      break;
    }

    Type* type = decl(tokens, NULL);
    switch (type->kind) {
      case TY_FUNC:
        type = inherit_decl(new_ptr_type(type), type);
        break;
      case TY_ARRAY:
        type = inherit_decl(new_ptr_type(type->base), type);
        break;
      default: {
      }
    }

    cur = cur->next = copy_type(type);
  }

  if (cur == &head) {
    type->is_variadic = true;
  }

  return new_func_type(type, head.next, is_variadic);
}

static Type* array_dimensions(Token** tokens, Type* type) {
  expect_token(tokens, "[");

  // Ignore those for now
  while (consume_token(tokens, "static") || consume_token(tokens, "restrict")) {}

  if (consume_token(tokens, "]")) {
    type = type_suffix(tokens, type);
    return new_array_type(type, -1);
  }

  int len = const_expr(tokens);
  expect_token(tokens, "]");

  type = type_suffix(tokens, type);

  return new_array_type(type, len);
}

static Node* func_args(Token** tokens, Type* type) {
  expect_token(tokens, "(");

  Node head = {};
  Node* cur = &head;
  Type* param = type->params;
  while (!consume_token(tokens, ")")) {
    if (cur != &head) {
      expect_token(tokens, ",");
    }

    if (!param && !type->is_variadic) {
      error_token(*tokens, "too many arguments");
    }

    Node* arg = assign(tokens);

    if (param) {
      if (param->kind != TY_STRUCT && param->kind != TY_UNION) {
        arg = new_cast_node(param, arg->token, arg);
      }
      param = param->next;
    } else if (arg->type->kind == TY_FLOAT) {
      arg = new_cast_node(new_double_type(), arg->token, arg);
    }

    cur = cur->next = arg;
  }

  if (param) {
    error_token(*tokens, "too few arguments");
  }

  return head.next;
}
