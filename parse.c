#include <stdbool.h>
#include "cc.h"

typedef struct Scope Scope;
typedef struct Initer Initer;
typedef struct DesignatedIniter DesignatedIniter;

struct Scope {
  Scope* next;
  Map first_class_objs;
  Map second_class_objs;
};

typedef struct VarAttr {
  bool is_typedef;
  bool is_extern;
  bool is_static;
  bool is_inline;
  bool is_thread_local;

  int alignment;
} VarAttr;

typedef struct TypeAttr {
  bool is_packed;
  int alignment;
} TypeAttr;

struct Initer {
  Initer* next;
  Type* type;
  Token* token;
  bool is_flexible;
  Node* expr;
  Member* mem;
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
static Obj* alloca_func;
static Scope* current_scope;
static Obj* current_lvars;
static Node* current_switch;
static Node* current_labels;
static Node* current_label_unresolved_nodes;
static char* current_break_label_id;
static char* current_continue_label_id;
static Obj* current_func;

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

static Member* new_member(Type* type, int index) {
  Member* mem = calloc(1, sizeof(Member));
  mem->type = type;
  mem->token = type->ident;
  mem->index = index;
  mem->name = type->name;
  mem->alignment = type->alignment;
  return mem;
}

static TopLevelObj* new_top_level_obj(Obj* obj) {
  TopLevelObj* objs = calloc(1, sizeof(TopLevelObj));
  objs->obj = obj;
  return objs;
}

static void add_code(Obj* code) {
  if (code->kind != OJ_FUNC && code->kind != OJ_GVAR) {
    UNREACHABLE("expected a top level object but got %d", code->kind);
  }

  TopLevelObj* top_level = new_top_level_obj(code);
  top_level->next = codes;
  codes = top_level;
}

static void enter_scope(void) {
  Scope* next = calloc(1, sizeof(Scope));
  next->next = current_scope;
  current_scope = next;
}

static void leave_scope(void) {
  if (!current_scope) {
    UNREACHABLE("no scipe to leave");
  }

  current_scope = current_scope->next;
}

static void add_first_class_obj_to_scope(Scope* scope, char* key, Obj* obj) {
  switch (obj->kind) {
    case OJ_FUNC:
    case OJ_GVAR:
    case OJ_LVAR:
    case OJ_ENUM:
    case OJ_DEF_TYPE:
      put_to_map(&scope->first_class_objs, key, obj);
      break;
    default:
      UNREACHABLE("expected a first class object but got %d", obj->kind);
  }
}

static void add_second_class_obj_to_scope(Scope* scope, char* key, Obj* obj) {
  if (obj->kind != OJ_TAG) {
    UNREACHABLE("expected a second class object but got %d", obj->kind);
  }

  put_to_map(&scope->second_class_objs, key, obj);
}

static void add_func_obj_to_scope(Scope* scope, char* key, Obj* func) {
  if (func->kind != OJ_FUNC) {
    UNREACHABLE("expected a function but got %d", func->kind);
  }

  add_first_class_obj_to_scope(scope, key, func);
}

static void add_var_obj_to_scope(Scope* scope, char* key, Obj* var) {
  if (var->kind != OJ_GVAR && var->kind != OJ_LVAR) {
    UNREACHABLE("expected a variable but got %d", var->kind);
  }

  add_first_class_obj_to_scope(scope, key, var);
}

static void add_enum_obj_to_scope(Scope* scope, char* key, Obj* enu) {
  if (enu->kind != OJ_ENUM) {
    UNREACHABLE("expected an enum but got %d", enu->kind);
  }

  add_first_class_obj_to_scope(scope, key, enu);
}

static void add_def_type_obj_to_scope(Scope* scope, char* key, Obj* def_type) {
  if (def_type->kind != OJ_DEF_TYPE) {
    UNREACHABLE("expected a defined type but got %d", def_type->kind);
  }

  add_first_class_obj_to_scope(scope, key, def_type);
}

static void add_tag_obj_to_scope(Scope* scope, char* key, Obj* tag) {
  if (tag->kind != OJ_TAG) {
    UNREACHABLE("expected a tag but got %d", tag->kind);
  }

  add_second_class_obj_to_scope(scope, key, tag);
}

static bool is_gscope(Scope* scope) {
  return !scope->next;
}

static void add_func_obj(Obj* func) {
  if (func->kind != OJ_FUNC) {
    UNREACHABLE("expected a function but got %d", func->kind);
  }

  add_func_obj_to_scope(current_scope, func->name, func);
}

static void add_gvar_obj(Obj* var) {
  if (var->kind != OJ_GVAR) {
    UNREACHABLE("expected a global variable but got %d", var->kind);
  }
  if (!is_gscope(gscope)) {
    UNREACHABLE("expected the global scope");
  }

  add_var_obj_to_scope(gscope, var->name, var);
}

static void add_var_obj_to_current_local_scope(char* key, Obj* var) {
  if (is_gscope(current_scope)) {
    UNREACHABLE("expected a local scope");
  }

  var->next = current_lvars;
  current_lvars = var;
  add_var_obj_to_scope(current_scope, key, var);
}

static void add_lvar_obj(Obj* var) {
  if (var->kind != OJ_LVAR) {
    UNREACHABLE("expected a local variable but got %d", var->kind);
  }

  add_var_obj_to_current_local_scope(var->name, var);
}

static void add_tag_obj(Obj* tag) {
  if (tag->kind != OJ_TAG) {
    UNREACHABLE("expected a tag but got %d", tag->kind);
  }

  add_tag_obj_to_scope(current_scope, tag->name, tag);
}

static void add_def_type_obj(Obj* def_type) {
  if (def_type->kind != OJ_DEF_TYPE) {
    UNREACHABLE("expected a defined type but got %d", def_type->kind);
  }

  add_def_type_obj_to_scope(current_scope, def_type->name, def_type);
}

static void add_enum_obj(Obj* enu) {
  if (enu->kind != OJ_ENUM) {
    UNREACHABLE("expected an enum but got %d", enu->kind);
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

static Obj* create_str_obj(Type* type, char* val) {
  Obj* str = create_stray_gvar_obj(type, new_id());
  str->is_static = true;

  // The val may contain \0, so use memcpy instead of strdup.
  str->val = calloc(type->len + 1, type->base->size);
  memcpy(str->val, val, type->size);

  add_code(str);
  return str;
}

static Obj* create_static_lvar_obj(Type* type, char* name) {
  Obj* var = create_anon_gvar_obj(type);
  var->is_static = true;
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

static Obj* find_datum_obj_from(Scope* scope, char* name) {
  for (Scope* s = scope; s; s = s->next) {
    Obj* var = get_from_map(&s->first_class_objs, name);
    if (!var) {
      continue;
    }

    switch (var->kind) {
      case OJ_FUNC:
      case OJ_GVAR:
      case OJ_LVAR:
      case OJ_ENUM:
        return var;
      default:
        return NULL;
    }
  }

  return NULL;
}

static Obj* find_datum_obj_from_current_scope(char* name) {
  return find_datum_obj_from(current_scope, name);
}

static Obj* find_def_type_obj(char* name) {
  for (Scope* s = current_scope; s; s = s->next) {
    Obj* def_type = get_from_map(&s->first_class_objs, name);
    if (!def_type) {
      continue;
    }

    return def_type->kind == OJ_DEF_TYPE ? def_type : NULL;
  }

  return NULL;
}

static Obj* find_tag_obj_in_current_scope(char* name) {
  Obj* obj = get_from_map(&current_scope->second_class_objs, name);
  return obj && obj->kind == OJ_TAG ? obj : NULL;
}

static Obj* find_tag_obj(char* name) {
  for (Scope* s = current_scope; s; s = s->next) {
    Obj* obj = get_from_map(&s->second_class_objs, name);
    if (!obj) {
      continue;
    }

    return obj->kind == OJ_TAG ? obj : NULL;
  }

  return NULL;
}

static bool equal_to_decl_specifier(Token* token) {
  static Map map;

  if (map.cap == 0) {
    static char* specs[] = {
      "typedef",
      "extern",
      "static",
      "inline",
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
      "typeof",
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
    static int len = sizeof(specs) / sizeof(char*);

    for (int i = 0; i < len; i++) {
      put_to_map(&map, specs[i], (void*)true);
    }
  }

  char* name = strndup(token->loc, token->len);
  return get_from_map(&map, name) != NULL || find_def_type_obj(name) != NULL;
}

static bool equal_to_abstract_decl_start(Token* token) {
  return equal_to_token(token, "(") && equal_to_decl_specifier(token->next);
}

static void add_label(Node* l) {
  if (l->kind != ND_LABEL) {
    UNREACHABLE("expected a label but got %d", l->kind);
  }

  l->labels = current_labels;
  current_labels = l;
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

static void add_label_unresolved_node(Node* node) {
  if (node->kind != ND_GOTO && node->kind != ND_LABEL_ADDR) {
    UNREACHABLE("expected a goto or label address but got %d", node->kind);
  }

  node->label_unresolved_nodes = current_label_unresolved_nodes;
  current_label_unresolved_nodes = node;
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

static Node* new_func_node(Token* token, Obj* obj) {
  Node* node = new_node(ND_FUNC);
  node->type = obj->type;
  node->token = token;
  node->obj = obj;
  return node;
}

static Node* new_gvar_node(Token* token, Obj* obj) {
  Node* node = new_node(ND_GVAR);
  node->type = obj->type;
  node->token = token;
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
      UNREACHABLE("expected a variable but got %d", obj->kind);
      return NULL;
  }
}

static Node* new_member_node(Token* token, Node* lhs, Member* mem) {
  Node* node = new_unary_node(ND_MEMBER, lhs);
  node->type = mem->type;
  node->token = token;
  node->mem = mem;
  return node;
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

static Node* new_atomic_cas_node(Token* token, Node* addr, Node* old_val, Node* new_val) {
  if (addr->type->kind != TY_PTR) {
    error_token(addr->token, "pointer expected");
  }
  if (old_val->type->kind != TY_PTR) {
    error_token(old_val->token, "pointer expected");
  }

  Node* node = new_node(ND_ATOMIC_CAS);
  node->type = new_bool_type();
  node->token = token;
  node->cas_addr = addr;
  node->cas_old_val = old_val;
  node->cas_new_val = new_val;
  return node;
}

static Node* new_atomic_exch_node(Token* token, Node* lhs, Node* rhs) {
  if (lhs->type->kind != TY_PTR) {
    error_token(lhs->token, "pointer expected");
  }

  Node* node = new_binary_node(ND_ATOMIC_EXCH, lhs, rhs);
  node->type = lhs->type;
  node->token = token;
  return node;
}

static Node* new_int_node(Token* token, int64_t val) {
  Node* node = new_node(ND_NUM);
  node->type = new_int_type();
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
  node->type = new_double_type();
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

static Node* create_label_addr_node(Token* token, char* label) {
  Node* addr = new_node(ND_LABEL_ADDR);
  addr->type = new_ptr_type(new_void_type());
  addr->token = token;
  addr->label = label;
  add_label_unresolved_node(addr);
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

  // int + ptr -> ptr + int
  if (is_int_type(lhs->type) && rhs->type->base) {
    Node* tmp = lhs;
    lhs = rhs;
    rhs = tmp;
  }

  // ptr + int
  if (lhs->type->base->kind == TY_VL_ARRAY) {
    rhs = new_mul_node(rhs->token, rhs, new_var_node(rhs->token, lhs->type->base->v_size));
  } else {
    rhs = new_mul_node(rhs->token, rhs, new_long_node(rhs->token, lhs->type->base->size));
  }
  Node* add = new_binary_node(ND_ADD, lhs, rhs);
  add->token = token;
  add->type = add->lhs->type;
  return add;
}

static Node* new_sub_node(Token* token, Node* lhs, Node* rhs) {
  // num - ptr
  if (is_numeric_type(lhs->type) && is_pointable_type(rhs->type)) {
    error_token(token, "invalid operands");
  }

  // num - num
  if (is_numeric_type(lhs->type) && is_numeric_type(rhs->type)) {
    usual_arith_convert(&lhs, &rhs);
    Node* sub = new_binary_node(ND_SUB, lhs, rhs);
    sub->token = token;
    sub->type = sub->lhs->type;
    return sub;
  }

  // ptr - ptr
  if (is_pointable_type(lhs->type) && is_pointable_type(rhs->type)) {
    Node* sub = new_binary_node(ND_SUB, lhs, rhs);
    sub->token = token;
    sub->type = new_long_type();
    return new_div_node(sub->token, sub, new_int_node(lhs->token, sub->lhs->type->base->size));
  }

  // ptr - int
  if (lhs->type->base->kind == TY_VL_ARRAY) {
    rhs = new_mul_node(rhs->token, rhs, new_var_node(rhs->token, lhs->type->base->v_size));
  } else {
    rhs = new_mul_node(rhs->token, rhs, new_long_node(rhs->token, lhs->type->base->size));
  }
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

static Node* new_shorthand_assigner_node(Token* token, NodeKind op, Node* lhs, Node* rhs) {
  switch (op) {
    case ND_BITOR:
      return new_bitor_node(token, lhs, rhs);
    case ND_BITXOR:
      return new_bitxor_node(token, lhs, rhs);
    case ND_BITAND:
      return new_bitand_node(token, lhs, rhs);
    case ND_LSHIFT:
      return new_lshift_node(token, lhs, rhs);
    case ND_RSHIFT:
      return new_rshift_node(token, lhs, rhs);
    case ND_ADD:
      return new_add_node(token, lhs, rhs);
    case ND_SUB:
      return new_sub_node(token, lhs, rhs);
    case ND_MUL:
      return new_mul_node(token, lhs, rhs);
    case ND_DIV:
      return new_div_node(token, lhs, rhs);
    case ND_MOD:
      return new_mod_node(token, lhs, rhs);
    default:
      error_token(token, "invalid operation");
      return NULL;
  }
}

static Node* new_shorthand_assign_node(Token* token, NodeKind op, Node* lhs, Node* rhs) {
  Node* node = new_shorthand_assigner_node(token, op, lhs, rhs);
  return new_assign_node(lhs->token, lhs, node);
}

static Node* new_comma_node(Token* token, Node* lhs, Node* rhs) {
  Node* comma = new_binary_node(ND_COMMA, lhs, rhs);
  comma->token = token;
  comma->type = comma->rhs->type;
  return comma;
}

static Node* new_vla_size_node(Token* token, Type* type) {
  Node* node = new_null_node(token);

  if (type->base) {
    node = new_comma_node(token, node, new_vla_size_node(token, type->base));
  }

  if (type->kind != TY_VL_ARRAY) {
    return node;
  }

  Node* base_size = NULL;
  if (type->base->kind == TY_VL_ARRAY) {
    base_size = new_var_node(token, type->base->v_size);
  } else {
    base_size = new_int_node(token, type->base->size);
  }

  type->v_size = create_anon_lvar_obj(new_ulong_type());
  Node* assign = new_assign_node(token, new_var_node(token, type->v_size), new_mul_node(token, base_size, type->v_len));

  return new_comma_node(token, node, assign);
}

static Node* new_alloca_call_node(Node* node) {
  return new_funccall_node(node->token, new_func_node(node->token, alloca_func), node);
}

static Node* new_expr_stmt_node(Node* lhs) {
  Node* stmt = new_unary_node(ND_EXPR_STMT, lhs);
  stmt->type = lhs->type;
  return stmt;
}

static Node* create_label_node(Token* token, char* label, Node* lhs) {
  Node* node = new_unary_node(ND_LABEL, lhs);
  node->token = token;
  node->type = lhs->type;
  node->label = label;
  node->label_id = new_id();
  add_label(node);
  return node;
}

static Node* create_goto_node(Token* token, char* label) {
  Node* g = new_node(ND_GOTO);
  g->token = token;
  g->label = label;
  add_label_unresolved_node(g);
  return g;
}

static Node* new_goto_label_addr_node(Token* token, Node* lhs) {
  Node* node = new_unary_node(ND_GOTO, lhs);
  node->token = token;
  return node;
}

static Node* new_contextual_goto_node(Token* token, char* label_id) {
  Node* node = new_node(ND_GOTO);
  node->token = token;
  node->label_id = label_id;
  return node;
}

static Node* new_asm_node(Token* token, char* str) {
  Node* node = new_node(ND_ASM);
  node->token = token;
  node->asm_str = str;
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
static Type* struct_decl(Token** tokens);
static Type* union_decl(Token** tokens);
static Type* enum_specifier(Token** tokens);
static Type* typeof_specifier(Token** tokens);
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
static int64_t eval_reloc(Node* node, char*** label);
static bool is_const_expr(Node* node);
static void gvar_initer(Token** tokens, Obj* var);
static Node* lvar_initer(Token** tokens, Obj* var);
static Initer* initer(Token** tokens, Type** type);

static void declare_builtin_funcs(void) {
  alloca_func = create_func_obj(new_func_type(new_ptr_type(new_void_type()), new_int_type(), false), "alloca");
}

bool is_func_declarator(Token* token, Type* type) {
  if (equal_to_token(token, ";")) {
    return false;
  }

  type = declarator(&token, type);
  return type->kind == TY_FUNC;
}

static void mark_referred_func(Obj* func) {
  if (!func || func->is_referred) {
    return;
  }
  if (func->kind != OJ_FUNC) {
    UNREACHABLE("expected a function but got %d", func->kind);
  }

  func->is_referred = true;

  for (Str* name = func->refering_funcs; name; name = name->next) {
    Obj* ref = find_datum_obj_from(gscope, name->data);
    if (!ref) {
      continue;
    }

    mark_referred_func(ref);
  }
}

static void mark_referred_funcs() {
  for (TopLevelObj* func = codes; func; func = func->next) {
    if (func->obj->kind != OJ_FUNC) {
      continue;
    }
    if (!func->obj->may_be_referred) {
      continue;
    }

    mark_referred_func(func->obj);
  }
}

void resolve_tentative_gvars() {
  for (TopLevelObj* var = codes; var; var = var->next) {
    if (var->obj->kind != OJ_GVAR) {
      continue;
    }
    if (!var->obj->is_tentative) {
      continue;
    }

    for (TopLevelObj* other = codes; other; other = other->next) {
      if (!equal_to_str(var->obj->name, other->obj->name)) {
        continue;
      }
      if (var == other || !other->obj->is_definition) {
        continue;
      }

      var->obj->is_definition = false;
      break;
    }
  }
}

TopLevelObj* parse(Token* token) {
  enter_scope();
  gscope = current_scope;

  declare_builtin_funcs();

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

  mark_referred_funcs();
  resolve_tentative_gvars();

  return codes;
}

static void resolve_labels(void) {
  for (Node* node = current_label_unresolved_nodes; node; node = node->label_unresolved_nodes) {
    for (Node* label = current_labels; label; label = label->labels) {
      if (!equal_to_str(node->label, label->label)) {
        continue;
      }

      node->label_id = label->label_id;
      break;
    }

    if (node->label_id == NULL) {
      error_token(node->token->next, "use of undeclared label");
    }
  }

  current_labels = NULL;
  current_label_unresolved_nodes = NULL;
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

static void create_auto_lvars(Token* token, Obj* func, Type* type) {
  // The return values whose types are struct or union and whose size are larger than 16 bytes
  // are returned by the callee's filling those values via the pointers to those values,
  // so assign the stack of the callee for the pointers.
  if (is_composite_type(type->return_type) && type->return_type->size > 16) {
    func->ptr_to_return_val = new_var_node(token, create_anon_lvar_obj(new_ptr_type(type->return_type)));
  }

  func->params = new_func_params(type->params);

  if (type->is_variadic) {
    func->va_area = create_lvar_obj(new_chars_type(136), "__va_area__");
  }

  func->alloca_bottom = create_lvar_obj(new_ptr_type(new_char_type()), "__alloca_size__");

  create_static_str_lvar_obj("__func__", func->name);
  create_static_str_lvar_obj("__FUNCTION__", func->name);
}

static void func(Token** tokens) {
  VarAttr attr = {};
  Type* type = decl(tokens, &attr);
  if (!type->name) {
    error_token(type->ident, "function name omitted");
  }

  Obj* func = find_datum_obj_from_current_scope(type->name);
  if (func) {
    if (func->kind != OJ_FUNC) {
      error_token(type->ident, "redeclared as a different kind of symbol");
    }

    bool is_definition = !equal_to_token(*tokens, ";");
    if (func->is_definition && is_definition) {
      error_token(type->ident, "redefinition of %s", type->name);
    }
    if (!func->is_static && attr.is_static) {
      error_token(type->ident, "static declaration follows non static declaration");
    }

    func->is_definition = func->is_definition || is_definition;
  } else {
    func = create_func_obj(type, type->name);
    func->is_static = attr.is_static || (attr.is_inline && !attr.is_extern);
    func->is_definition = !equal_to_token(*tokens, ";");
  }

  func->may_be_referred = !(attr.is_static && attr.is_inline);

  if (consume_token(tokens, ";")) {
    current_func = current_lvars = NULL;
    return;
  }

  current_func = func;

  enter_scope();

  // As the type may differ from the one declared before (func->type),
  // pass the type parsed this time as well.
  create_auto_lvars(*tokens, func, type);

  func->body = block_stmt(tokens);

  leave_scope();

  func->lvars = current_lvars;

  current_lvars = NULL;
  resolve_labels();
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

static Relocation* new_reloc(char** label, int offset, long addend) {
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
    case TY_UNION: {
      Member* mem = init->mem ? init->mem : init->type->members;
      return write_gvar_data(data, offset, init->children[mem->index], reloc);
    }
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

      char** label = NULL;
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
    var->is_thread_local = attr.is_thread_local;
    var->is_definition = !attr.is_extern;

    if (consume_token(tokens, "=")) {
      gvar_initer(tokens, var);
    } else if (!var->is_thread_local && var->is_definition) {
      // If there is no assignment for this variable, this declarations may or may not be the definition.
      var->is_tentative = true;
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
    type->is_defined = true;
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

      // [GCC] nested functions
      if (is_func_declarator(peeked, spec)) {
        Obj* prev_func = current_func;
        Obj* prev_lvars = current_lvars;

        func(tokens);

        current_func = prev_func;
        current_lvars = prev_lvars;
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

  expect_tokens(tokens, "if", "(", NULL);

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

  expect_tokens(tokens, "switch", "(", NULL);

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

  node->case_begin = const_expr(tokens);

  // [GNU] case range
  if (consume_token(tokens, "...")) {
    node->case_end = const_expr(tokens);
    if (node->case_end < node->case_begin) {
      error_token(start, "empty case range specified");
    }
  } else {
    node->case_end = node->case_begin;
  }

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

  expect_tokens(tokens, "for", "(", NULL);

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

  expect_tokens(tokens, "while", "(", NULL);

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

  Node* node = new_node(ND_DO);
  node->token = start;

  char* prev_break_label_id = renew_break_label_id(&node->break_label_id);
  char* prev_continue_label_id = renew_continue_label_id(&node->continue_label_id);

  node->then = stmt(tokens);

  current_break_label_id = prev_break_label_id;
  current_continue_label_id = prev_continue_label_id;

  expect_tokens(tokens, "while", "(", NULL);

  node->cond = expr(tokens);

  expect_tokens(tokens, ")", ";", NULL);

  return node;
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

  // [GNU] goto statement to a label address
  if (consume_token(tokens, "*")) {
    Node* lhs = expr(tokens);
    expect_token(tokens, ";");

    return new_goto_label_addr_node(start, lhs);
  }

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

  expect_token(tokens, ";");

  return new_contextual_goto_node(start, current_break_label_id);
}

static Node* continue_stmt(Token** tokens) {
  Token* start = *tokens;

  expect_token(tokens, "continue");

  if (!current_continue_label_id) {
    error_token(start, "stray continue");
  }

  expect_token(tokens, ";");

  return new_contextual_goto_node(start, current_continue_label_id);
}

static Node* asm_stmt(Token** tokens) {
  Token* start = *tokens;

  expect_token(tokens, "asm");

  while (consume_token(tokens, "inline") || consume_token(tokens, "volatile")) {}

  expect_token(tokens, "(");

  if ((*tokens)->kind != TK_STR || (*tokens)->type->base->kind != TY_CHAR) {
    error_token(start, "expected string literal");
  }
  Node* node = new_asm_node(start, (*tokens)->str_val);

  *tokens = (*tokens)->next;

  expect_tokens(tokens, ")", ";", NULL);

  return node;
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

  if (equal_to_token(*tokens, "asm")) {
    return asm_stmt(tokens);
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

// Convert A op= B which is atomic to
// ({
//    TA* addr = &A; TB val = (B); TA old_val = *addr; TA new_val;
//    do {
//      new_val = old_val op val;
//    } while (!atomic_compare_exchange_strong(addr, &old_val, new_val));
//    new_val;
// })
static Node* convert_to_atomic_assign_node(Token* token, NodeKind op, Node* lhs, Node* rhs) {
  Node head = {};
  Node* cur = &head;

  Node* addr = new_var_node(token, create_anon_lvar_obj(new_ptr_type(lhs->type)));
  cur = cur->next = new_assign_node(token, addr, new_addr_node(token, lhs));
  Node* val = new_var_node(token, create_anon_lvar_obj(rhs->type));
  cur = cur->next = new_assign_node(token, val, rhs);
  Node* old_val = new_var_node(token, create_anon_lvar_obj(lhs->type));
  cur = cur->next = new_assign_node(token, old_val, new_deref_node(token, addr));
  Node* new_val = new_var_node(token, create_anon_lvar_obj(lhs->type));

  cur = cur->next = new_node(ND_DO);
  cur->token = token;

  char* prev_break_label_id = renew_break_label_id(&cur->break_label_id);
  char* prev_continue_label_id = renew_continue_label_id(&cur->continue_label_id);

  Node* assign = new_assign_node(token, new_val, new_shorthand_assigner_node(token, op, old_val, val));
  cur->then = new_expr_stmt_node(assign);

  current_break_label_id = prev_break_label_id;
  current_continue_label_id = prev_continue_label_id;

  Node* cas = new_atomic_cas_node(token, addr, new_addr_node(token, old_val), new_val);
  cur->cond = new_not_node(token, cas);

  cur = cur->next = new_val;

  return new_stmt_expr_node(token, head.next);
}

static Node* convert_to_assign_node(Token* token, NodeKind op, Node* lhs, Node* rhs) {
  // When the assignee is the member of a composite type,
  // convert A.x op= B to tmp = &A, (*tmp).x = (*tmp).x op B
  // as it is not possible to reference the member if it is bitfield.
  if (lhs->kind == ND_MEMBER) {
    Node* tmp = new_var_node(token, create_anon_lvar_obj(new_ptr_type(lhs->lhs->type)));
    Node* tmp_assign = new_assign_node(token, tmp, new_addr_node(token, lhs->lhs));

    Node* tmp_member = new_member_node(token, new_deref_node(token, tmp), lhs->mem);

    Node* assign = new_shorthand_assign_node(token, op, tmp_member, rhs);

    return new_comma_node(token, tmp_assign, assign);
  }

  if (lhs->type->is_atomic) {
    return convert_to_atomic_assign_node(token, op, lhs, rhs);
  }

  // Otherwise, convert A op= B to tmp = &A, *tmp = *tmp op B.
  Node* tmp = new_var_node(token, create_anon_lvar_obj(new_ptr_type(lhs->type)));
  Node* tmp_assign = new_assign_node(token, tmp, new_addr_node(token, lhs));

  Node* tmp_deref = new_deref_node(token, tmp);

  Node* assign = new_shorthand_assign_node(token, op, tmp_deref, rhs);

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

    // Convert A ?: B to tmp = A, tmp ? tmp : B in order not to re-evaluate the A node.
    if (equal_to_token(*tokens, ":")) {
      Node* tmp_var = new_var_node(start, create_anon_lvar_obj(node->type));
      Node* tmp_assign = new_assign_node(start, tmp_var, node);
      expect_token(tokens, ":");
      Node* els = conditional(tokens);
      return new_comma_node(start, tmp_assign, new_cond_node(start, tmp_var, tmp_var, els));
    }

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

static Node* generic_select(Token** tokens) {
  Token* start = *tokens;

  expect_tokens(tokens, "_Generic", "(", NULL);

  Node* control = assign(tokens);
  expect_token(tokens, ",");

  Type* control_type = control->type;
  switch (control->type->kind) {
    case TY_FUNC:
      control_type = new_ptr_type(control_type);
      break;
    case TY_ARRAY:
      control_type = new_ptr_type(control_type->base);
      break;
    default: {
    }
  }

  Node* selected = NULL;
  bool is_first = true;
  while (!consume_token(tokens, ")")) {
    if (!is_first) {
      expect_token(tokens, ",");
    }
    is_first = false;

    if (consume_token(tokens, "default")) {
      expect_token(tokens, ":");
      Node* node = assign(tokens);
      if (!selected) {
        selected = node;
      }
      continue;
    }

    Type* type = abstract_decl(tokens, NULL);
    expect_token(tokens, ":");
    Node* node = assign(tokens);
    if (is_type_compatible_with(control_type, type)) {
      selected = node;
    }
  }

  if (!selected) {
    error_token(start, "controlling expression type not compatible with any generic accosited type");
  }

  return selected;
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
    Node* lhs = cast(tokens);
    if (lhs->kind == ND_MEMBER && lhs->mem->is_bitfield) {
      error_token(start, "cannot take the address of bitfield");
    }
    return new_addr_node(start, lhs);
  }

  if (consume_token(tokens, "&&")) {
    Token* label = expect_ident_token(tokens);
    return create_label_addr_node(start, strndup(label->loc, label->len));
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
    expect_tokens(tokens, "sizeof", "(", NULL);
    Type* type = abstract_decl(tokens, NULL);
    expect_token(tokens, ")");

    if (type->kind != TY_VL_ARRAY) {
      return new_ulong_node(start, type->size);
    }

    if (type->v_size) {
      return new_var_node(start, type->v_size);
    }

    Node* size = new_vla_size_node(start, type);
    Node* var = new_var_node(start, type->v_size);
    return new_comma_node(start, size, var);
  }

  if (consume_token(tokens, "sizeof")) {
    Node* node = unary(tokens);
    return node->type->kind == TY_VL_ARRAY ? new_var_node(start, node->type->v_size)
                                           : new_ulong_node(start, node->type->size);
  }

  if (equal_to_token(*tokens, "_Alignof") && equal_to_abstract_decl_start((*tokens)->next)) {
    expect_tokens(tokens, "_Alignof", "(", NULL);
    Type* type = abstract_decl(tokens, NULL);
    expect_token(tokens, ")");
    return new_ulong_node(start, type->alignment);
  }

  if (consume_token(tokens, "_Alignof")) {
    Node* node = unary(tokens);
    return new_ulong_node(start, node->type->alignment);
  }

  if (equal_to_token(*tokens, "_Generic")) {
    return generic_select(tokens);
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

  if (consume_token(tokens, "__builtin_types_compatible_p")) {
    expect_token(tokens, "(");
    Type* left = abstract_decl(tokens, NULL);
    expect_token(tokens, ",");
    Type* right = abstract_decl(tokens, NULL);
    expect_token(tokens, ")");
    return new_int_node(start, is_type_compatible_with(left, right));
  }

  if (consume_token(tokens, "__builtin_compare_and_swap")) {
    expect_token(tokens, "(");
    Node* addr = assign(tokens);
    expect_token(tokens, ",");
    Node* old_val = assign(tokens);
    expect_token(tokens, ",");
    Node* new_val = assign(tokens);
    expect_token(tokens, ")");
    return new_atomic_cas_node(start, addr, old_val, new_val);
  }

  if (consume_token(tokens, "__builtin_atomic_exchange")) {
    expect_token(tokens, "(");
    Node* lhs = assign(tokens);
    expect_token(tokens, ",");
    Node* rhs = assign(tokens);
    expect_token(tokens, ")");
    return new_atomic_exch_node(start, lhs, rhs);
  }

  return postfix(tokens);
}

static Node* increment(Node* node, int delta) {
  Node* inc = convert_to_assign_node(node->token, ND_ADD, node, new_int_node(node->token, delta));
  return new_cast_node(node->type, node->token, new_add_node(node->token, inc, new_int_node(node->token, -delta)));
}

static Member* find_member_from(Member* mems, char* c, int len) {
  for (Member* mem = mems; mem; mem = mem->next) {
    if (is_composite_type(mem->type) && !mem->name) {
      if (find_member_from(mem->type->members, c, len)) {
        return mem;
      }
      continue;
    }

    if (!equal_to_n_chars(mem->name, c, len)) {
      continue;
    }

    return mem;
  }

  return NULL;
}

static Node* member(Token** tokens, Token* token, Node* lhs) {
  if (!is_composite_type(lhs->type)) {
    error_token(*tokens, "expected a struct or union");
  }

  Token* ident = expect_ident_token(tokens);

  Member* mems = lhs->type->members;
  Node* node = lhs;
  for (;;) {
    Member* mem = find_member_from(mems, ident->loc, ident->len);
    if (!mem) {
      error_token(*tokens, "no such member");
    }

    node = new_member_node(ident, node, mem);

    if (mem->name) {
      break;
    }
    mems = mem->type->members;
  }

  return node;
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
  Obj* datum = find_datum_obj_from_current_scope(strndup(ident->loc, ident->len));
  if (!datum) {
    if (equal_to_token(*tokens, "(")) {
      error_token(ident, "implicit declaration of a function");
    } else {
      error_token(ident, "undefined variable");
    }
  }

  switch (datum->kind) {
    case OJ_FUNC:
      if (current_func) {
        add_str(&current_func->refering_funcs, new_str(datum->name));
      } else {
        // A function referred in global scope such as a function
        // in the initializer of a global array of functions may be referred as well.
        datum->may_be_referred = true;
      }
      return new_func_node(ident, datum);
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

  if (equal_to_tokens(*tokens, "(", "{", NULL)) {
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
    node->type = (*tokens)->type;
    *tokens = (*tokens)->next;
    return node;
  }

  if ((*tokens)->kind == TK_STR) {
    Obj* str = create_str_obj((*tokens)->type, (*tokens)->str_val);
    Node* node = new_var_node(*tokens, str);
    *tokens = (*tokens)->next;
    return node;
  }

  error_token(start, "expected a primary but got %d", start->kind);
  return NULL;
}

static int64_t relocate(Node* node, char*** label) {
  switch (node->kind) {
    case ND_FUNC:
    case ND_GVAR:
      *label = &node->obj->name;
      return 0;
    case ND_LVAR:
      error_token(node->token, "not a compile-time constant");
      return 0;
    case ND_DEREF:
      return eval_reloc(node->lhs, label);
    case ND_MEMBER:
      return relocate(node->lhs, label) + node->mem->offset;
    default:
      error_token(node->token, "invalid initializer: expected a relocatable but got %d", node->kind);
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

static int64_t eval_reloc(Node* node, char*** label) {
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
    case ND_LABEL_ADDR:
      *label = &node->label_id;
      return 0;
    case ND_NOT:
      return !eval(node->lhs);
    case ND_BITNOT:
      return ~eval(node->lhs);
    case ND_FUNC:
    case ND_GVAR:
    case ND_LVAR:
      if (!label) {
        error_token(node->token, "not a compile-time constant");
      }
      if (node->type->kind != TY_FUNC && node->type->kind != TY_ARRAY) {
        error_token(node->token, "invalid initializer: expected a function or an array but got : %d", node->type->kind);
      }

      *label = &node->obj->name;
      return 0;
    case ND_MEMBER:
      if (!label) {
        error_token(node->token, "not a compile-time constant");
      }
      if (node->type->kind != TY_ARRAY) {
        error_token(node->token, "invalid initializer: expected an array but got : %d", node->type->kind);
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

static bool is_const_expr(Node* node) {
  switch (node->kind) {
    case ND_COMMA:
      return is_const_expr(node->rhs);
    case ND_COND:
      if (!is_const_expr(node->cond)) {
        return false;
      }
      return is_const_expr(eval(node->cond) ? node->then : node->els);
    case ND_LOGOR:
    case ND_LOGAND:
    case ND_BITOR:
    case ND_BITXOR:
    case ND_BITAND:
    case ND_EQ:
    case ND_NE:
    case ND_LT:
    case ND_LE:
    case ND_LSHIFT:
    case ND_RSHIFT:
    case ND_ADD:
    case ND_SUB:
    case ND_MUL:
    case ND_DIV:
    case ND_MOD:
      return is_const_expr(node->lhs) && is_const_expr(node->rhs);
    case ND_CAST:
    case ND_NEG:
    case ND_NOT:
    case ND_BITNOT:
      return is_const_expr(node->lhs);
    case ND_NUM:
      return true;
    default:
      return false;
  }
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
    for (Member* mem = init->type->members; mem; mem = mem->next) {
      init->children[mem->index] = new_initer(mem->type);
      if (!mem->next && type->is_flexible) {
        init->children[mem->index]->is_flexible = true;
      }
    }
    return init;
  }

  if (init->type->kind == TY_ARRAY) {
    if (init->type->size < 0) {
      init->is_flexible = true;
      return init;
    }

    init->children = calloc(init->type->len, sizeof(Initer*));
    for (int i = 0; i < init->type->len; i++) {
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
  return equal_to_token(token, "}") || equal_to_tokens(token, ",", "}", NULL);
}

static bool consume_initer_end(Token** tokens) {
  if (consume_token(tokens, "}")) {
    return true;
  }

  if (equal_to_tokens(*tokens, ",", "}", NULL)) {
    expect_tokens(tokens, ",", "}", NULL);
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
  int len = (*tokens)->type->len;

  if (init->is_flexible) {
    *init = *new_initer(new_array_type(init->type->base, len));
  }

  if (init->type->len < len) {
    len = init->type->len;
  }

  switch (init->type->base->size) {
    case 1: {
      char* val = (*tokens)->str_val;
      for (int i = 0; i < len; i++) {
        init->children[i]->expr = new_int_node(*tokens, val[i]);
      }
      break;
    }
    case 2: {
      uint16_t* val = (uint16_t*)(*tokens)->str_val;
      for (int i = 0; i < len; i++) {
        init->children[i]->expr = new_int_node(*tokens, val[i]);
      }
      break;
    }
    case 4: {
      uint32_t* val = (uint32_t*)(*tokens)->str_val;
      for (int i = 0; i < len; i++) {
        init->children[i]->expr = new_int_node(*tokens, val[i]);
      }
      break;
    }
    default:
      error_token(*tokens, "invalid string initer");
  }

  *tokens = (*tokens)->next;
}

static Member* composite_designator(Token** tokens, Type* type) {
  Token* start = *tokens;

  expect_token(tokens, ".");

  Token* ident = expect_ident_token(tokens);
  Member* mem = find_member_from(type->members, ident->loc, ident->len);
  if (!mem) {
    error_token(ident, "struct or union has no such member");
  }
  if (mem->type->kind == TY_STRUCT && !mem->name) {
    // Rrever tokens if the designator is the member of an anonymous struct so that the member can be initialized as
    // usual through the initialization of the anonymous struct at the later step as this function is used in the
    // initialization of the parent of the anonymous struct in this case.
    *tokens = start;
  }

  return mem;
}

static void array_designator(Token** tokens, Type* type, bool is_flexible, int* begin, int* end) {
  Token* start = *tokens;

  expect_token(tokens, "[");

  *begin = const_expr(tokens);

  // [GNU] range designator
  if (consume_token(tokens, "...")) {
    *end = const_expr(tokens);
  } else {
    *end = *begin;
  }
  if (*end < *begin) {
    error_token(start, "array designator [%d, %d] is empty", *begin, *end);
  }
  if (!is_flexible && *end >= type->len) {
    error_token(start, "array designator index exceeds array bounds");
  }

  expect_token(tokens, "]");
}

static void init_direct_struct_initer(Token** tokens, Initer* init, Member* from);
static void init_direct_array_initer(Token** tokens, Initer* init, int from);

static void init_designated_initer(Token** tokens, Initer* init) {
  if (equal_to_token(*tokens, ".")) {
    switch (init->type->kind) {
      case TY_STRUCT: {
        Member* mem = composite_designator(tokens, init->type);
        // Reset the initial value and re-initialize with the designated value.
        init->expr = NULL;
        init_designated_initer(tokens, init->children[mem->index]);
        init_direct_struct_initer(tokens, init, mem->next);
        return;
      }
      case TY_UNION: {
        Member* mem = composite_designator(tokens, init->type);
        init_designated_initer(tokens, init->children[mem->index]);
        return;
      }
      default:
        error_token(*tokens, "field name in non struct or union initializer");
        return;
    }
  }

  if (equal_to_token(*tokens, "[")) {
    if (init->type->kind != TY_ARRAY) {
      error_token(*tokens, "array index in non array initializer");
    }
    int begin = 0;
    int end = 0;
    array_designator(tokens, init->type, init->is_flexible, &begin, &end);
    for (int i = begin; i <= end; i++) {
      Token* assign = *tokens;
      init_designated_initer(&assign, init->children[i]);
      if (i == end) {
        *tokens = assign;
      }
    }
    init_direct_array_initer(tokens, init, end + 1);
    return;
  }

  consume_token(tokens, "=");
  init_initer(tokens, init);
}

static void init_struct_initer(Token** tokens, Initer* init) {
  expect_token(tokens, "{");

  bool is_first = true;
  for (Member* mem = init->type->members; !consume_initer_end(tokens);) {
    if (!is_first) {
      expect_token(tokens, ",");
    }
    is_first = false;

    if (equal_to_token(*tokens, ".")) {
      mem = composite_designator(tokens, init->type);
      init_designated_initer(tokens, init->children[mem->index]);
      mem = mem->next;
      continue;
    }

    if (mem) {
      init_initer(tokens, init->children[mem->index]);
      mem = mem->next;
      continue;
    }

    skip_excess_initers(tokens);
  }
}

static void init_direct_struct_initer(Token** tokens, Initer* init, Member* from) {
  for (Member* mem = from; mem && !equal_to_initer_end(*tokens); mem = mem->next) {
    Token* start = *tokens;

    if (mem->index > 0) {
      expect_token(tokens, ",");
    }

    // When it is the designated initializer, revert tokens and
    // let the parent continue initializing with the tokens.
    if (equal_to_token(*tokens, "[") || equal_to_token(*tokens, ".")) {
      *tokens = start;
      return;
    }

    init_initer(tokens, init->children[mem->index]);
  }
}

static void init_union_initer(Token** tokens, Initer* init) {
  if (equal_to_tokens(*tokens, "{", ".", NULL)) {
    expect_token(tokens, "{");
    init->mem = composite_designator(tokens, init->type);
    init_designated_initer(tokens, init->children[init->mem->index]);
    expect_initer_end(tokens);
    return;
  }

  init->mem = init->type->members;

  if (consume_token(tokens, "{")) {
    init_initer(tokens, init->children[init->mem->index]);
    expect_initer_end(tokens);
    return;
  }

  init_initer(tokens, init->children[init->mem->index]);
}

static int count_initers(Token* token, Type* type) {
  Initer* ignored = new_initer(type);

  int i = 0;
  int len = 0;
  while (!consume_initer_end(&token)) {
    if (i > 0) {
      expect_token(&token, ",");
    }

    if (equal_to_token(token, "[")) {
      int begin = 0;
      int end = 0;
      array_designator(&token, type, true, &begin, &end);
      i = end;
      init_designated_initer(&token, ignored);
    } else {
      init_initer(&token, ignored);
    }

    i++;
    len = MAX(len, i);
  }
  return len;
}

static void init_array_initer(Token** tokens, Initer* init) {
  expect_token(tokens, "{");

  if (init->is_flexible) {
    int len = count_initers(*tokens, init->type->base);
    *init = *new_initer(new_array_type(init->type->base, len));
    init->is_flexible = false;
  }

  bool is_first = true;
  for (int i = 0; !consume_initer_end(tokens); i++) {
    if (!is_first) {
      expect_token(tokens, ",");
    }
    is_first = false;

    if (equal_to_token(*tokens, "[")) {
      int begin = 0;
      int end = 0;
      array_designator(tokens, init->type, init->is_flexible, &begin, &end);
      for (int i = begin; i <= end; i++) {
        Token* assign = *tokens;
        init_designated_initer(&assign, init->children[i]);
        if (i == end) {
          *tokens = assign;
        }
      }
      i = end;
      continue;
    }

    if (i < init->type->len) {
      init_initer(tokens, init->children[i]);
      continue;
    }

    skip_excess_initers(tokens);
  }
}

static void init_direct_array_initer(Token** tokens, Initer* init, int from) {
  if (init->is_flexible) {
    int len = count_initers(*tokens, init->type->base);
    *init = *new_initer(new_array_type(init->type->base, len));
    init->is_flexible = false;
  }

  for (int i = from; i < init->type->len && !equal_to_initer_end(*tokens); i++) {
    Token* start = *tokens;

    if (i > 0) {
      expect_token(tokens, ",");
    }

    // When it is the designated initializer, revert tokens and
    // let the parent continue initializing with the tokens.
    if (equal_to_token(*tokens, "[") || equal_to_token(*tokens, ".")) {
      *tokens = start;
      return;
    }

    init_initer(tokens, init->children[i]);
  }
}

static void init_initer(Token** tokens, Initer* init) {
  switch (init->type->kind) {
    case TY_STRUCT: {
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

      init_direct_struct_initer(tokens, init, init->type->members);
      return;
    }
    case TY_UNION:
      init_union_initer(tokens, init);
      return;
    case TY_ARRAY:
      if ((*tokens)->kind == TK_STR) {
        init_string_initer(tokens, init);
        return;
      }

      if (equal_to_token(*tokens, "{")) {
        init_array_initer(tokens, init);
        return;
      }

      init_direct_array_initer(tokens, init, 0);
      return;
    default:
      if (consume_token(tokens, "{")) {
        init->expr = assign(tokens);
        expect_token(tokens, "}");
        return;
      }

      init->expr = assign(tokens);
      return;
  }
}

static Initer* initer(Token** tokens, Type** type) {
  Initer* init = new_initer(*type);
  init_initer(tokens, init);

  if (is_composite_type(init->type) && init->type->is_flexible) {
    Type* copied = copy_composite_type(*type, init->type->kind);

    Member* mem = copied->members;
    while (mem->next) {
      mem = mem->next;
    }
    mem->type = init->children[mem->index]->type;
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
  switch (init->type->kind) {
    case TY_STRUCT:
      if (init->expr) {
        break;
      }

      Node* node = new_null_node(token);

      for (Member* mem = init->type->members; mem; mem = mem->next) {
        DesignatedIniter next = {designated, 0, NULL, mem};
        node = new_comma_node(token, node, lvar_init(token, init->children[mem->index], &next));
      }

      return node;
    case TY_UNION: {
      Member* mem = init->mem ? init->mem : init->type->members;
      DesignatedIniter next = {designated, 0, NULL, mem};
      return lvar_init(token, init->children[mem->index], &next);
    }
    case TY_ARRAY: {
      Node* node = new_null_node(token);

      for (int i = 0; i < init->type->len; i++) {
        DesignatedIniter next = {designated, i, NULL};
        node = new_comma_node(token, node, lvar_init(token, init->children[i], &next));
      }

      return node;
    }
    default: {
    }
  }

  return init->expr ? new_assign_node(token, designated_expr(token, designated), init->expr) : new_null_node(token);
}

static Node* lvar_initer(Token** tokens, Obj* var) {
  Token* start = *tokens;

  Initer* init = initer(tokens, &var->type);

  DesignatedIniter designated = {NULL, 0, var};

  return new_comma_node(start, new_memzero_node(start, var), lvar_init(start, init, &designated));
}

static Node* lvar_decl(Token** tokens) {
  Token* start = *tokens;

  VarAttr attr = {};
  Type* spec = decl_specifier(tokens, &attr);

  Node head = {};
  Node* cur = &head;
  bool is_first = true;
  while (!equal_to_token(*tokens, ";")) {
    if (!is_first) {
      expect_token(tokens, ",");
    }
    is_first = false;

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

    // Check if the type consists of a variable length array (VLA),
    // and calculate the size of it if the type is VLA, otherwise do nothing.
    cur = cur->next = new_expr_stmt_node(new_vla_size_node(*tokens, type));

    if (type->kind == TY_VL_ARRAY) {
      if (equal_to_token(*tokens, "=")) {
        error_token(*tokens, "vaiable sized object may not be initialized");
      }

      // Unlike array, the address allocated for VLA match not the offset of this variable from the base pointer
      // but the assigned value by the alloca call in this assignment, so set the type of this variable as a pointer
      // to the VLA so that it can load and store the address of VLA as other variable whose types are pointers do
      // normally.
      Node* var = new_var_node(type->ident, create_lvar_obj(type, type->name));
      var->type = new_ptr_type(var->type);
      Node* call = new_alloca_call_node(new_var_node(type->ident, type->v_size));
      cur = cur->next = new_assign_node(*tokens, var, call);
      continue;
    }

    Obj* var = create_lvar_obj(type, type->name);

    if (!consume_token(tokens, "=")) {
      continue;
    }

    Node* node = lvar_initer(tokens, var);
    if (var->type->size < 0) {
      error_token(type->ident, "variable has imcomplete type");
    }

    cur = cur->next = node;
  }

  return new_block_node(start, head.next);
}

static Member* members(Token** tokens, bool* is_flexible) {
  Member head = {};
  Member* cur = &head;
  int i = 0;
  *is_flexible = false;
  while (!consume_token(tokens, "}")) {
    VarAttr attr = {};
    Type* spec = decl_specifier(tokens, &attr);

    // Anonymous composite member
    if (is_composite_type(spec) && equal_to_token(*tokens, ";")) {
      Member* mem = new_member(spec, i++);
      mem->token = *tokens;

      expect_token(tokens, ";");

      if (attr.alignment) {
        mem->alignment = attr.alignment;
      }
      cur = cur->next = mem;
      continue;
    }

    bool is_first = true;
    while (!consume_token(tokens, ";")) {
      if (!is_first) {
        expect_token(tokens, ",");
      }
      is_first = false;

      Type* type = declarator(tokens, spec);

      Member* mem = new_member(type, i++);
      if (attr.alignment) {
        mem->alignment = attr.alignment;
      }

      if (consume_token(tokens, ":")) {
        mem->is_bitfield = true;
        mem->bit_width = const_expr(tokens);
      }

      if (!mem->is_bitfield && !type->name) {
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

static void attribute(Token** tokens, TypeAttr* attr) {
  while (consume_token(tokens, "__attribute__")) {
    expect_tokens(tokens, "(", "(", NULL);

    bool is_first = true;
    while (!consume_token(tokens, ")")) {
      if (!is_first) {
        expect_token(tokens, ",");
      }
      is_first = false;

      if (consume_token(tokens, "packed")) {
        attr->is_packed = true;
        continue;
      }

      if (consume_token(tokens, "aligned")) {
        expect_token(tokens, "(");
        attr->alignment = const_expr(tokens);
        expect_token(tokens, ")");
        continue;
      }
    }

    expect_token(tokens, ")");
  }
}

static Type* struct_decl(Token** tokens) {
  expect_token(tokens, "struct");

  TypeAttr attr = {};
  attribute(tokens, &attr);

  Token* tag = NULL;
  if ((*tokens)->kind == TK_IDENT) {
    tag = expect_ident_token(tokens);
  }

  if (tag && !equal_to_token(*tokens, "{")) {
    char* name = strndup(tag->loc, tag->len);
    Obj* found = find_tag_obj(name);
    if (found) {
      return found->type;
    }

    Type* struc = new_struct_type(-1, 1, NULL);
    create_tag_obj(struc, name);
    return struc;
  }

  expect_token(tokens, "{");

  bool is_flexible = false;
  Member* mems = members(tokens, &is_flexible);
  attribute(tokens, &attr);

  int bits = 0;
  int alignment = attr.alignment != 0 ? attr.alignment : 1;
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
      if (!attr.is_packed) {
        bits = align_up(bits, mem->alignment * 8);
      }
      mem->offset = bits / 8;
      bits += mem->type->size * 8;
    }

    if (!attr.is_packed && alignment < mem->alignment) {
      alignment = mem->alignment;
    }
  }

  int size = align_up(bits, alignment * 8) / 8;
  Type* struc = new_struct_type(size, alignment, mems);
  struc->is_flexible = is_flexible;

  if (!tag) {
    return struc;
  }

  Obj* found = find_tag_obj_in_current_scope(strndup(tag->loc, tag->len));
  if (found) {
    *found->type = *struc;
    return found->type;
  }

  create_tag_obj(struc, strndup(tag->loc, tag->len));
  return struc;
}

static Type* union_decl(Token** tokens) {
  expect_token(tokens, "union");

  TypeAttr attr = {};
  attribute(tokens, &attr);

  Token* tag = NULL;
  if ((*tokens)->kind == TK_IDENT) {
    tag = expect_ident_token(tokens);
  }

  if (tag && !equal_to_token(*tokens, "{")) {
    char* name = strndup(tag->loc, tag->len);
    Obj* found = find_tag_obj(name);
    if (found) {
      return found->type;
    }

    Type* uni = new_union_type(-1, 1, NULL);
    create_tag_obj(uni, name);
    return uni;
  }

  expect_token(tokens, "{");

  bool is_flexible = false;
  Member* mems = members(tokens, &is_flexible);
  attribute(tokens, &attr);

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

  Obj* found = find_tag_obj_in_current_scope(strndup(tag->loc, tag->len));
  if (found) {
    *found->type = *uni;
    return found->type;
  }

  create_tag_obj(uni, strndup(tag->loc, tag->len));
  return uni;
}

static Type* enum_specifier(Token** tokens) {
  expect_token(tokens, "enum");

  Token* tag = NULL;
  if ((*tokens)->kind == TK_IDENT) {
    tag = expect_ident_token(tokens);
  }

  if (tag && !equal_to_token(*tokens, "{")) {
    Obj* found = find_tag_obj(strndup(tag->loc, tag->len));
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

static Type* typeof_specifier(Token** tokens) {
  expect_tokens(tokens, "typeof", "(", NULL);

  Type* type = NULL;
  if (equal_to_decl_specifier(*tokens)) {
    type = abstract_decl(tokens, NULL);
  } else {
    Node* node = expr(tokens);
    type = node->type;
  }

  expect_token(tokens, ")");

  return type;
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
  bool is_atomic = false;
  int spec = 0;

  while (equal_to_decl_specifier(*tokens)) {
    Token* start = *tokens;

    if (equal_to_any_token(*tokens, "typedef", "extern", "static", "inline", "_Thread_local", "__thread", NULL)) {
      if (!attr) {
        error_token(*tokens, "storage class specifier is not allowed in this context");
      }

      if (equal_to_token(*tokens, "typedef")) {
        attr->is_typedef = true;
      }
      if (equal_to_token(*tokens, "extern")) {
        attr->is_extern = true;
      }
      if (equal_to_token(*tokens, "static")) {
        attr->is_static = true;
      }
      if (equal_to_token(*tokens, "inline")) {
        attr->is_inline = true;
      }
      if (equal_to_any_token(*tokens, "_Thread_local", "__thread", NULL)) {
        attr->is_thread_local = true;
      }

      if (attr->is_typedef && attr->is_extern + attr->is_static + attr->is_inline >= 2) {
        error_token(*tokens, "typedef may not be used together with extern, static or inline");
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

    if (consume_token(tokens, "_Atomic")) {
      if (consume_token(tokens, "(")) {
        type = abstract_decl(tokens, NULL);
        expect_token(tokens, ")");
      }
      is_atomic = true;
      continue;
    }

    if (equal_to_token(*tokens, "_Alignas")) {
      if (!attr) {
        error_token(*tokens, "_Alignas is not allowed in this context");
      }

      expect_tokens(tokens, "_Alignas", "(", NULL);
      if (equal_to_decl_specifier(*tokens)) {
        Type* type = abstract_decl(tokens, NULL);
        attr->alignment = type->alignment;
      } else {
        attr->alignment = const_expr(tokens);
      }
      expect_token(tokens, ")");
      continue;
    }

    Obj* def_type = find_def_type_obj(strndup((*tokens)->loc, (*tokens)->len));
    if (def_type) {
      if (spec > 0) {
        break;
      }

      spec += OTHER;
      type = def_type->type;
      *tokens = (*tokens)->next;
      continue;
    }

    if (equal_to_token(*tokens, "struct")) {
      if (spec > 0) {
        break;
      }

      spec += OTHER;
      type = struct_decl(tokens);
      continue;
    }

    if (equal_to_token(*tokens, "union")) {
      if (spec > 0) {
        break;
      }

      spec += OTHER;
      type = union_decl(tokens);
      continue;
    }

    if (equal_to_token(*tokens, "enum")) {
      if (spec > 0) {
        break;
      }

      spec += OTHER;
      type = enum_specifier(tokens);
      continue;
    }

    if (equal_to_token(*tokens, "typeof")) {
      if (spec > 0) {
        break;
      }

      spec += OTHER;
      type = typeof_specifier(tokens);
      continue;
    }

    if (consume_token(tokens, "void")) {
      spec += VOID;
    }

    if (consume_token(tokens, "_Bool")) {
      spec += BOOL;
    }

    if (consume_token(tokens, "char")) {
      spec += CHAR;
    }

    if (consume_token(tokens, "short")) {
      spec += SHORT;
    }

    if (consume_token(tokens, "int")) {
      spec += INT;
    }

    if (consume_token(tokens, "long")) {
      spec += LONG;
    }

    if (consume_token(tokens, "float")) {
      spec += FLOAT;
    }

    if (consume_token(tokens, "double")) {
      spec += DOUBLE;
    }

    if (consume_token(tokens, "signed")) {
      spec |= SIGNED;
    }

    if (consume_token(tokens, "unsigned")) {
      spec |= UNSIGNED;
    }

    switch (spec) {
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
        type = new_double_type();
        break;
      case LONG + DOUBLE:
        type = new_ldouble_type();
        break;
      default: {
        error_token(start, "expected a typename");
      }
    }
  }

  type->is_atomic = is_atomic;

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

  if (equal_to_tokens(*tokens, "void", ")", NULL)) {
    expect_tokens(tokens, "void", ")", NULL);
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
    is_variadic = true;
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

  Node* len = conditional(tokens);
  expect_token(tokens, "]");

  type = type_suffix(tokens, type);

  if (type->kind == TY_VL_ARRAY || !is_const_expr(len)) {
    return new_vl_array_type(type, len);
  }

  return new_array_type(type, eval(len));
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
