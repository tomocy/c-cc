#include "cc.h"

typedef struct ScopedObj ScopedObj;
typedef struct Scope Scope;

typedef struct Decl {
  Type* type;
  Token* ident;
  char* name;
} Decl;

struct ScopedObj {
  ScopedObj* next;
  Obj* obj;
};

struct Scope {
  Scope* next;
  ScopedObj* first_class_objs;
  ScopedObj* second_class_objs;
};

static Obj* codes;
static Scope* gscope;
static Scope* current_scope;
static Obj* current_lvars;
static Node* current_switch;
static Node* current_labels;
static Node* current_gotos;
static char* current_break_label_id;
static char* current_continue_label_id;
static Obj* current_func;

static Type* ty_void = &(Type){
    TY_VOID,
    1,
    1,
};
static Type* ty_bool = &(Type){
    TY_BOOL,
    1,
    1,
};
static Type* ty_char = &(Type){
    TY_CHAR,
    1,
    1,
};
static Type* ty_short = &(Type){
    TY_SHORT,
    2,
    2,
};
static Type* ty_int = &(Type){
    TY_INT,
    4,
    4,
};
static Type* ty_long = &(Type){
    TY_LONG,
    8,
    8,
};

static char* new_id() {
  static int id = 0;
  char* name = calloc(20, sizeof(char));
  sprintf(name, ".L%d", id++);
  return name;
}

static Token* expect_ident(Token** tokens) {
  if ((*tokens)->kind != TK_IDENT) {
    error_token(*tokens, "expected an ident");
  }

  Token* ident = *tokens;
  *tokens = (*tokens)->next;
  return ident;
}

static int align(int n, int align) { return (n + align - 1) / align * align; }

static bool are_strs_equal_n(char* a, char* b, int b_len) {
  return strlen(a) == b_len && strncmp(a, b, b_len) == 0;
}

static bool are_strs_equal(char* a, char* b) {
  return are_strs_equal_n(a, b, strlen(b));
}

static Type* new_type(TypeKind kind, int size, int alignment);

static Type* new_ptr_type(Type* base) {
  Type* ptr = new_type(TY_PTR, 8, 8);
  ptr->base = base;
  return ptr;
}

static Type* new_array_type(Type* base, int len) {
  Type* arr = new_type(TY_ARRAY, base->size * len, base->alignment);
  arr->base = base;
  arr->len = len;
  return arr;
}

static Type* new_struct_type(int size, int alignment, Member* mems) {
  Type* struc = new_type(TY_STRUCT, align(size, alignment), alignment);
  struc->members = mems;
  return struc;
}

static Type* new_union_type(int size, int alignment, Member* mems) {
  Type* uni = new_type(TY_UNION, align(size, alignment), alignment);
  uni->members = mems;
  return uni;
}

static Type* new_type(TypeKind kind, int size, int alignment) {
  Type* type = calloc(1, sizeof(Type));
  type->kind = kind;
  type->size = size;
  type->alignment = alignment;
  return type;
}

bool is_numable(Type* type) {
  return type->kind == TY_BOOL || type->kind == TY_CHAR ||
         type->kind == TY_SHORT || type->kind == TY_INT ||
         type->kind == TY_LONG;
}

static bool is_pointable(Type* type) {
  return type->kind == TY_PTR || type->kind == TY_ARRAY;
}

static void enter_scope() {
  Scope* next = calloc(1, sizeof(Scope));
  next->next = current_scope;
  current_scope = next;
}

static void leave_scope() {
  if (!current_scope) {
    error("no scope to leave");
  }
  current_scope = current_scope->next;
}

static void add_code(Obj* code) {
  if (code->kind != OJ_FUNC && code->kind != OJ_GVAR) {
    error("expected a top level object");
  }
  code->next = codes;
  codes = code;
}

static void add_first_class_obj_to_scope(Scope* scope, Obj* obj) {
  if (obj->kind != OJ_GVAR && obj->kind != OJ_LVAR && obj->kind != OJ_ENUM &&
      obj->kind != OJ_DEF_TYPE) {
    error("expected a first class object");
  }

  ScopedObj* scoped = calloc(1, sizeof(ScopedObj));
  scoped->obj = obj;
  scoped->next = scope->first_class_objs;
  scope->first_class_objs = scoped;
}

static void add_second_class_obj_to_scope(Scope* scope, Obj* obj) {
  if (obj->kind != OJ_TAG) {
    error("expected a second class object");
  }

  ScopedObj* scoped = calloc(1, sizeof(ScopedObj));
  scoped->obj = obj;
  scoped->next = scope->second_class_objs;
  scope->second_class_objs = scoped;
}

static void add_var_to_scope(Scope* scope, Obj* var) {
  if (var->kind != OJ_GVAR && var->kind != OJ_LVAR) {
    error("expected a variable");
  }

  add_first_class_obj_to_scope(scope, var);
}

static void add_enum_to_scope(Scope* scope, Obj* enu) {
  if (enu->kind != OJ_ENUM) {
    error("expected an enum");
  }

  add_first_class_obj_to_scope(scope, enu);
}

static void add_def_type_to_scope(Scope* scope, Obj* def_type) {
  if (def_type->kind != OJ_DEF_TYPE) {
    error("expected a defined type");
  }

  add_first_class_obj_to_scope(scope, def_type);
}

static void add_tag_to_scope(Scope* scope, Obj* tag) {
  if (tag->kind != OJ_TAG) {
    error("expected a tag");
  }

  add_second_class_obj_to_scope(scope, tag);
}

static bool is_gscope(Scope* scope) { return !scope->next; }

static void add_gvar(Obj* var) {
  if (var->kind != OJ_GVAR) {
    error("expected a global var");
  }
  if (!is_gscope(gscope)) {
    error("expected a global scope");
  }
  add_var_to_scope(gscope, var);
}

static void add_lvar(Obj* var) {
  if (var->kind != OJ_LVAR) {
    error("expected a local var");
  }
  if (is_gscope(current_scope)) {
    error("expected a local scope");
  }
  var->next = current_lvars;
  current_lvars = var;
  add_var_to_scope(current_scope, var);
}

static void add_tag(Obj* tag) {
  if (tag->kind != OJ_TAG) {
    error("expected a tag");
  }
  add_tag_to_scope(current_scope, tag);
}

static void add_def_type(Obj* def_type) {
  if (def_type->kind != OJ_DEF_TYPE) {
    error("expected a defined type");
  }
  add_def_type_to_scope(current_scope, def_type);
}

static void add_enum(Obj* enu) {
  if (enu->kind != OJ_ENUM) {
    error("expected an enum");
  }
  add_enum_to_scope(current_scope, enu);
}

static Obj* new_obj(ObjKind kind) {
  Obj* obj = calloc(1, sizeof(Obj));
  obj->kind = kind;
  return obj;
}

static Obj* new_gvar(Type* type, char* name) {
  Obj* var = new_obj(OJ_GVAR);
  var->type = type;
  var->name = name;
  add_gvar(var);
  return var;
}

static Obj* new_str(char* name, char* val) {
  Type* ty = new_array_type(ty_char, strlen(val) + 1);
  Obj* str = new_gvar(ty, name);
  str->str_val = strdup(val);
  add_code(str);
  return str;
}

static Obj* new_lvar(Type* type, char* name) {
  Obj* var = new_obj(OJ_LVAR);
  var->type = type;
  var->name = name;
  var->offset =
      align((current_lvars) ? current_lvars->offset + type->size : type->size,
            type->alignment);
  add_lvar(var);
  return var;
}

static Obj* new_enum(char* name, int64_t val) {
  Obj* enu = new_obj(OJ_ENUM);
  enu->name = name;
  enu->int_val = val;
  add_enum(enu);
  return enu;
}

static Obj* new_def_type(Type* type, char* name) {
  Obj* def_type = new_obj(OJ_DEF_TYPE);
  def_type->type = type;
  def_type->name = name;
  add_def_type(def_type);
  return def_type;
}

static Obj* new_tag(Type* type, char* name) {
  Obj* tag = new_obj(OJ_TAG);
  tag->type = type;
  tag->name = name;
  add_tag(tag);
  return tag;
}

static Obj* find_func(char* name, int len) {
  for (Obj* func = codes; func; func = func->next) {
    if (!are_strs_equal_n(func->name, name, len)) {
      continue;
    }

    return func->kind == OJ_FUNC ? func : NULL;
  }
  return NULL;
}

static Obj* find_var_or_enum(char* name, int len) {
  for (Scope* s = current_scope; s; s = s->next) {
    for (ScopedObj* var = s->first_class_objs; var; var = var->next) {
      if (!are_strs_equal_n(var->obj->name, name, len)) {
        continue;
      }

      return var->obj->kind == OJ_GVAR || var->obj->kind == OJ_LVAR ||
                     var->obj->kind == OJ_ENUM
                 ? var->obj
                 : NULL;
    }
  }
  return NULL;
}

static Obj* find_def_type(char* name, int len) {
  for (Scope* s = current_scope; s; s = s->next) {
    for (ScopedObj* def_type = s->first_class_objs; def_type;
         def_type = def_type->next) {
      if (!are_strs_equal_n(def_type->obj->name, name, len)) {
        continue;
      }

      return def_type->obj->kind == OJ_DEF_TYPE ? def_type->obj : NULL;
    }
  }
  return NULL;
}

static Obj* find_tag_in_current_scope(char* name, int len) {
  for (ScopedObj* tag = current_scope->second_class_objs; tag;
       tag = tag->next) {
    if (!are_strs_equal_n(tag->obj->name, name, len)) {
      continue;
    }

    return tag->obj->kind == OJ_TAG ? tag->obj : NULL;
  }
  return NULL;
}

static Obj* find_tag(char* name, int len) {
  for (Scope* s = current_scope; s; s = s->next) {
    for (ScopedObj* tag = s->second_class_objs; tag; tag = tag->next) {
      if (!are_strs_equal_n(tag->obj->name, name, len)) {
        continue;
      }

      return tag->obj->kind == OJ_TAG ? tag->obj : NULL;
    }
  }
  return NULL;
}

bool equal_to_type_name(Token* token) {
  static char* names[] = {"void", "_Bool",  "char",  "short", "int",
                          "long", "struct", "union", "enum"};
  int len = sizeof(names) / sizeof(char*);
  for (int i = 0; i < len; i++) {
    if (equal_to_token(token, names[i])) {
      return true;
    }
  }
  return find_def_type(token->loc, token->len) != NULL;
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

static Node* new_member_node(Token** tokens, Token* token, Node* lhs) {
  if (lhs->type->kind != TY_STRUCT && lhs->type->kind != TY_UNION) {
    error_token(*tokens, "expected a struct or union");
  }

  Token* ident = expect_ident(tokens);

  for (Member* mem = lhs->type->members; mem; mem = mem->next) {
    if (!are_strs_equal_n(mem->name, ident->loc, ident->len)) {
      continue;
    }

    Node* node = new_unary_node(ND_MEMBER, lhs);
    node->type = mem->type;
    node->token = token;
    node->name = mem->name;
    node->offset = mem->offset;
    return node;
  }

  error_token(*tokens, "no such member");
  return NULL;
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

static Node* new_gvar_node(Type* type, Token* token, char* name) {
  Node* node = new_node(ND_GVAR);
  node->type = type;
  node->token = token;
  node->name = name;
  return node;
}

static Node* new_lvar_node(Type* type, Token* token, int offset) {
  Node* node = new_node(ND_LVAR);
  node->type = type;
  node->token = token;
  node->offset = offset;
  return node;
}

static Node* new_var_node(Token* token, Obj* obj) {
  switch (obj->kind) {
    case OJ_GVAR:
      return new_gvar_node(obj->type, token, obj->name);
    case OJ_LVAR:
      return new_lvar_node(obj->type, token, obj->offset);
    default:
      error_token(token, "expected a variable object");
      return NULL;
  }
}

static Node* new_funccall_node(Type* type, Token* token, char* name,
                               Node* args) {
  Node* node = new_node(ND_FUNCCALL);
  node->type = type;
  node->token = token;
  node->name = name;
  node->args = args;
  return node;
}

static Node* new_int_node(Token* token, int64_t val) {
  Node* node = new_node(ND_NUM);
  node->type = ty_int;
  node->token = token;
  node->val = val;
  return node;
}

static Node* new_long_node(Token* token, int64_t val) {
  Node* node = new_node(ND_NUM);
  node->type = ty_long;
  node->token = token;
  node->val = val;
  return node;
}

static Node* new_addr_node(Token* token, Node* lhs) {
  Node* addr = new_unary_node(ND_ADDR, lhs);
  addr->type = new_ptr_type(addr->lhs->type);
  addr->token = token;
  return addr;
}

static Node* new_deref_node(Token* token, Node* lhs) {
  Node* deref = new_unary_node(ND_DEREF, lhs);
  deref->type =
      (deref->lhs->type->base) ? deref->lhs->type->base : deref->lhs->type;
  deref->token = token;
  return deref;
}

static Node* new_not_node(Token* token, Node* lhs) {
  Node* n = new_unary_node(ND_NOT, lhs);
  n->type = ty_int;
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

static Type* get_common_type(Type* a, Type* b) {
  if (a->base) {
    return new_ptr_type(a->base);
  }

  if (a->size == ty_long->size || b->size == ty_long->size) {
    return ty_long;
  }

  return ty_int;
}

static void usual_arith_convert(Node** lhs, Node** rhs) {
  Type* common = get_common_type((*lhs)->type, (*rhs)->type);
  *lhs = new_cast_node(common, (*lhs)->token, *lhs);
  *rhs = new_cast_node(common, (*rhs)->token, *rhs);
}

static Node* new_mul_node(Node* lhs, Node* rhs) {
  usual_arith_convert(&lhs, &rhs);
  Node* mul = new_binary_node(ND_MUL, lhs, rhs);
  mul->type = lhs->type;
  mul->token = lhs->token;
  return mul;
}

static Node* new_div_node(Node* lhs, Node* rhs) {
  usual_arith_convert(&lhs, &rhs);
  Node* div = new_binary_node(ND_DIV, lhs, rhs);
  div->type = lhs->type;
  div->token = lhs->token;
  return div;
}

static Node* new_mod_node(Node* lhs, Node* rhs) {
  usual_arith_convert(&lhs, &rhs);
  Node* mod = new_binary_node(ND_MOD, lhs, rhs);
  mod->type = lhs->type;
  mod->token = lhs->token;
  return mod;
}

static Node* new_add_node(Token* tokens, Node* lhs, Node* rhs) {
  if (is_pointable(lhs->type) && is_pointable(rhs->type)) {
    error_token(tokens, "invalid operands");
  }

  if (is_numable(lhs->type) && is_numable(rhs->type)) {
    usual_arith_convert(&lhs, &rhs);
    Node* add = new_binary_node(ND_ADD, lhs, rhs);
    add->type = add->lhs->type;
    return add;
  }

  if (is_pointable(lhs->type) && is_numable(rhs->type)) {
    rhs = new_mul_node(rhs, new_long_node(rhs->token, lhs->type->base->size));
    Node* add = new_binary_node(ND_ADD, lhs, rhs);
    add->type = add->lhs->type;
    return add;
  }

  lhs = new_mul_node(lhs, new_long_node(lhs->token, rhs->type->base->size));
  Node* add = new_binary_node(ND_ADD, lhs, rhs);
  add->type = add->lhs->type;
  return add;
}

static Node* new_sub_node(Token* tokens, Node* lhs, Node* rhs) {
  if (is_numable(lhs->type) && is_pointable(rhs->type)) {
    error_token(tokens, "invalid operands");
  }

  if (is_pointable(lhs->type) && is_pointable(rhs->type)) {
    Node* sub = new_binary_node(ND_SUB, lhs, rhs);
    sub->type = sub->lhs->type;
    return new_div_node(sub,
                        new_int_node(lhs->token, sub->lhs->type->base->size));
  }

  if (is_numable(lhs->type) && is_numable(rhs->type)) {
    usual_arith_convert(&lhs, &rhs);
    Node* sub = new_binary_node(ND_SUB, lhs, rhs);
    sub->type = sub->lhs->type;
    return sub;
  }

  rhs = new_mul_node(rhs, new_long_node(rhs->token, lhs->type->base->size));
  Node* sub = new_binary_node(ND_SUB, lhs, rhs);
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

static Node* new_eq_node(Node* lhs, Node* rhs) {
  usual_arith_convert(&lhs, &rhs);
  Node* eq = new_binary_node(ND_EQ, lhs, rhs);
  eq->type = ty_long;
  eq->token = lhs->token;
  return eq;
}

static Node* new_ne_node(Node* lhs, Node* rhs) {
  usual_arith_convert(&lhs, &rhs);
  Node* ne = new_binary_node(ND_NE, lhs, rhs);
  ne->type = ty_long;
  ne->token = lhs->token;
  return ne;
}

static Node* new_lt_node(Node* lhs, Node* rhs) {
  usual_arith_convert(&lhs, &rhs);
  Node* lt = new_binary_node(ND_LT, lhs, rhs);
  lt->type = ty_long;
  lt->token = lhs->token;
  return lt;
}

static Node* new_le_node(Node* lhs, Node* rhs) {
  usual_arith_convert(&lhs, &rhs);
  Node* le = new_binary_node(ND_LE, lhs, rhs);
  le->type = ty_long;
  le->token = le->token;
  return le;
}

static Node* new_bitand_node(Node* lhs, Node* rhs) {
  usual_arith_convert(&lhs, &rhs);
  Node* n = new_binary_node(ND_BITAND, lhs, rhs);
  n->type = lhs->type;
  n->token = lhs->token;
  return n;
}

static Node* new_bitxor_node(Node* lhs, Node* rhs) {
  usual_arith_convert(&lhs, &rhs);
  Node* n = new_binary_node(ND_BITXOR, lhs, rhs);
  n->type = lhs->type;
  n->token = lhs->token;
  return n;
}

static Node* new_bitor_node(Node* lhs, Node* rhs) {
  usual_arith_convert(&lhs, &rhs);
  Node* n = new_binary_node(ND_BITOR, lhs, rhs);
  n->type = lhs->type;
  n->token = lhs->token;
  return n;
}

static Node* new_and_node(Node* lhs, Node* rhs) {
  usual_arith_convert(&lhs, &rhs);
  Node* n = new_binary_node(ND_AND, lhs, rhs);
  n->type = ty_int;
  n->token = lhs->token;
  return n;
}

static Node* new_or_node(Node* lhs, Node* rhs) {
  usual_arith_convert(&lhs, &rhs);
  Node* n = new_binary_node(ND_OR, lhs, rhs);
  n->type = ty_int;
  n->token = lhs->token;
  return n;
}

static Node* new_cond_node(Token* token, Node* cond, Node* then, Node* els) {
  Node* c = new_node(ND_COND);
  c->token = token;
  c->cond = cond;
  c->then = then;
  c->els = els;

  if (c->then->type->kind == TY_VOID || c->els->type->kind == TY_VOID) {
    c->type = ty_void;
  } else {
    usual_arith_convert(&c->then, &c->els);
    c->type = c->then->type;
  }
  return c;
}

static Node* new_assign_node(Node* lhs, Node* rhs) {
  if (lhs->type->kind != TY_STRUCT) {
    rhs = new_cast_node(lhs->type, rhs->token, rhs);
  }
  Node* assign = new_binary_node(ND_ASSIGN, lhs, rhs);
  assign->type = lhs->type;
  assign->token = lhs->token;
  return assign;
}

static Node* new_comma_node(Node* lhs, Node* rhs) {
  Node* comma = new_binary_node(ND_COMMA, lhs, rhs);
  comma->type = comma->rhs->type;
  comma->token = rhs->token;
  return comma;
}

static Node* new_expr_stmt_node(Node* lhs) {
  Node* stmt = new_unary_node(ND_EXPR_STMT, lhs);
  stmt->type = lhs->type;
  return stmt;
}

static Node* new_label_node(Token* token, char* label, Node* lhs) {
  Node* l = new_unary_node(ND_LABEL, lhs);
  l->token = token;
  l->label = label;
  l->label_id = new_id();
  add_label(l);
  return l;
}

static Node* new_goto_node(Token* token, char* label) {
  Node* g = new_node(ND_GOTO);
  g->token = token;
  g->label = label;
  add_goto(g);
  return g;
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
static Node* orr(Token** tokens);
static Node* andd(Token** tokens);
static Node* bitorr(Token** tokens);
static Node* bitxorr(Token** tokens);
static Node* bitandd(Token** tokens);
static Node* equality(Token** tokens);
static Node* relational(Token** tokens);
static Node* shift(Token** tokens);
static Node* add(Token** tokens);
static Node* mul(Token** tokens);
static Node* cast(Token** tokens);
static Node* unary(Token** tokens);
static Node* postfix(Token** tokens);
static Node* primary(Token** tokens);
static Node* lvar(Token** tokens);
static Node* var_decl(Token** tokens);
static Type* enum_specifier(Token** tokens);
static Type* struct_decl(Token** tokens);
static Type* union_decl(Token** tokens);
static Member* members(Token** tokens);
static Type* decl_specifier(Token** tokens);
static Decl* declarator(Token** tokens, Type* type);
static Decl* abstract_declarator(Token** tokens, Type* type);
static Type* type_suffix(Token** tokens, Type* type);
static Type* array_dimensions(Token** tokens, Type* type);
static Node* func_args(Token** tokens, Node* params);

bool is_func(Token* tokens) {
  consume_token(&tokens, "static");
  decl_specifier(&tokens);

  if (tokens->kind != TK_IDENT) {
    error_token(tokens, "expected an ident");
  }
  tokens = tokens->next;
  return equal_to_token(tokens, "(");
}

Obj* parse(Token* tokens) {
  enter_scope();
  gscope = current_scope;

  while (tokens->kind != TK_EOF) {
    if (equal_to_token(tokens, "typedef")) {
      tydef(&tokens);
      continue;
    }

    if (is_func(tokens)) {
      func(&tokens);
      continue;
    }
    gvar(&tokens);
  }

  leave_scope();

  return codes;
}

static void resolve_goto_labels() {
  for (Node* g = current_gotos; g; g = g->gotos) {
    for (Node* l = current_labels; l; l = l->labels) {
      if (!are_strs_equal(g->label, l->label)) {
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

static void func(Token** tokens) {
  bool is_static = consume_token(tokens, "static");

  Type* ty = decl_specifier(tokens);

  Token* ident = expect_ident(tokens);

  Obj* func = new_obj(OJ_FUNC);
  current_func = func;
  add_code(func);
  func->type = ty;
  func->name = strndup(ident->loc, ident->len);
  func->is_static = is_static;

  expect_token(tokens, "(");
  enter_scope();

  Node head = {};
  Node* cur = &head;
  while (!consume_token(tokens, ")")) {
    if (cur != &head) {
      expect_token(tokens, ",");
    }

    Decl* decl = declarator(tokens, decl_specifier(tokens));
    if (decl->type->kind == TY_ARRAY) {
      decl->type = new_ptr_type(decl->type->base);
    }

    Obj* var = new_lvar(decl->type, decl->name);
    Node* param = new_var_node(decl->ident, var);
    cur->next = param;
    cur = cur->next;
  }
  func->params = head.next;

  func->is_definition = !consume_token(tokens, ";");
  if (!func->is_definition) {
    return;
  }

  func->body = block_stmt(tokens);

  leave_scope();

  func->lvars = current_lvars;
  func->stack_size = align((func->lvars) ? func->lvars->offset : 0, 16);
  current_lvars = NULL;
  resolve_goto_labels();
  current_func = NULL;
}

static void gvar(Token** tokens) {
  Type* spec = decl_specifier(tokens);

  bool is_first = true;
  while (!consume_token(tokens, ";")) {
    if (!is_first) {
      expect_token(tokens, ",");
    }
    is_first = false;

    Decl* decl = declarator(tokens, spec);
    if (decl->type->size < 0) {
      error_token(decl->ident, "variable has imcomplete type");
    }

    Obj* var = new_gvar(decl->type, decl->name);
    add_code(var);

    if (consume_token(tokens, "=")) {
      var->int_val = expect_num(tokens);
    }
  }
}

static void tydef(Token** tokens) {
  expect_token(tokens, "typedef");

  Type* spec = decl_specifier(tokens);

  bool is_first = true;
  while (!consume_token(tokens, ";")) {
    if (!is_first) {
      expect_token(tokens, ",");
    }
    is_first = false;

    Decl* decl = declarator(tokens, spec);
    new_def_type(decl->type, decl->name);
  }
}

static Node* block_stmt(Token** tokens) {
  Token* start = *tokens;

  expect_token(tokens, "{");
  enter_scope();

  Node head = {};
  Node* curr = &head;
  while (!consume_token(tokens, "}")) {
    if (equal_to_token(*tokens, "typedef")) {
      tydef(tokens);
      continue;
    }

    curr->next = stmt(tokens);
    curr = curr->next;
  }

  leave_scope();

  Node* node = new_node(ND_BLOCK);
  node->token = start;
  node->body = head.next;
  return node;
}

static Node* if_stmt(Token** tokens) {
  Token* start = *tokens;

  expect_token(tokens, "if");
  expect_token(tokens, "(");

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

  expect_token(tokens, "switch");
  expect_token(tokens, "(");

  Node* prev_switch = current_switch;
  Node* node = current_switch = new_node(ND_SWITCH);
  node->token = start;

  char* prev_break_label_id = current_break_label_id;
  node->break_label_id = current_break_label_id = new_id();

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

  node->val = expect_num(tokens);
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

static Node* while_stmt(Token** tokens) {
  Token* start = *tokens;

  expect_token(tokens, "while");
  expect_token(tokens, "(");

  Node* node = new_node(ND_FOR);
  node->token = start;

  char* prev_break_label_id = current_break_label_id;
  node->break_label_id = current_break_label_id = new_id();
  char* prev_continue_label_id = current_continue_label_id;
  node->continue_label_id = current_continue_label_id = new_id();

  node->cond = expr(tokens);

  expect_token(tokens, ")");

  node->then = stmt(tokens);

  current_break_label_id = prev_break_label_id;
  current_continue_label_id = prev_continue_label_id;

  return node;
}

static Node* for_stmt(Token** tokens) {
  Token* start = *tokens;

  expect_token(tokens, "for");
  expect_token(tokens, "(");

  Node* node = new_node(ND_FOR);
  node->token = start;

  enter_scope();

  char* prev_break_label_id = current_break_label_id;
  node->break_label_id = current_break_label_id = new_id();
  char* prev_continue_label_id = current_continue_label_id;
  node->continue_label_id = current_continue_label_id = new_id();

  if (!consume_token(tokens, ";")) {
    if (equal_to_type_name(*tokens)) {
      node->init = var_decl(tokens);
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

static Node* return_stmt(Token** tokens) {
  Token* start = *tokens;

  expect_token(tokens, "return");

  Node* node = new_unary_node(
      ND_RETURN, new_cast_node(current_func->type, *tokens, expr(tokens)));
  node->token = start;

  expect_token(tokens, ";");

  return node;
}

static Node* label_stmt(Token** tokens) {
  Token* start = *tokens;

  Token* ident = expect_ident(tokens);
  expect_token(tokens, ":");

  return new_label_node(start, strndup(ident->loc, ident->len), stmt(tokens));
}

static Node* goto_stmt(Token** tokens) {
  Token* start = *tokens;

  expect_token(tokens, "goto");

  Token* ident = expect_ident(tokens);
  expect_token(tokens, ";");

  return new_goto_node(start, strndup(ident->loc, ident->len));
}

static Node* break_stmt(Token** tokens) {
  Token* start = *tokens;

  expect_token(tokens, "break");

  if (!current_break_label_id) {
    error_token(start, "stray break");
  }

  Node* node = new_node(ND_GOTO);
  node->token = start;
  node->label_id = current_break_label_id;
  return node;
}

static Node* continue_stmt(Token** tokens) {
  Token* start = *tokens;

  expect_token(tokens, "continue");

  if (!current_continue_label_id) {
    error_token(start, "stray continue");
  }

  Node* node = new_node(ND_GOTO);
  node->token = start;
  node->label_id = current_continue_label_id;
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

  if (equal_to_token(*tokens, "while")) {
    return while_stmt(tokens);
  }

  if (equal_to_token(*tokens, "for")) {
    return for_stmt(tokens);
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

  if (equal_to_type_name(*tokens)) {
    return lvar(tokens);
  }

  return expr_stmt(tokens);
}

static Node* expr_stmt(Token** tokens) {
  Token* start = *tokens;

  if (consume_token(tokens, ";")) {
    Node* node = new_node(ND_BLOCK);
    node->token = start;
    return node;
  }

  Node* node = new_expr_stmt_node(expr(tokens));
  expect_token(tokens, ";");
  return node;
}

static Node* expr(Token** tokens) {
  Node* node = assign(tokens);
  if (consume_token(tokens, ",")) {
    return new_comma_node(node, expr(tokens));
  }
  return node;
}

static Node* convert_to_assign_node(Token* token, NodeKind op, Node* lhs,
                                    Node* rhs) {
  Obj* tmp_var = new_lvar(new_ptr_type(lhs->type), "");

  Node* tmp_assign = new_assign_node(new_var_node(lhs->token, tmp_var),
                                     new_addr_node(lhs->token, lhs));

  Node* tmp_deref =
      new_deref_node(lhs->token, new_var_node(lhs->token, tmp_var));
  Node* arith_op;
  switch (op) {
    case ND_BITOR:
      arith_op = new_bitor_node(tmp_deref, rhs);
      break;
    case ND_BITXOR:
      arith_op = new_bitxor_node(tmp_deref, rhs);
      break;
    case ND_BITAND:
      arith_op = new_bitand_node(tmp_deref, rhs);
      break;
    case ND_LSHIFT:
      arith_op = new_lshift_node(token, tmp_deref, rhs);
      break;
    case ND_RSHIFT:
      arith_op = new_rshift_node(token, tmp_deref, rhs);
      break;
    case ND_ADD:
      arith_op = new_add_node(token, tmp_deref, rhs);
      break;
    case ND_SUB:
      arith_op = new_sub_node(token, tmp_deref, rhs);
      break;
    case ND_MUL:
      arith_op = new_mul_node(tmp_deref, rhs);
      break;
    case ND_DIV:
      arith_op = new_div_node(tmp_deref, rhs);
      break;
    case ND_MOD:
      arith_op = new_mod_node(tmp_deref, rhs);
      break;
    default:
      error_token(token, "invalid operation");
  }

  Node* assign = new_assign_node(
      new_deref_node(lhs->token, new_var_node(lhs->token, tmp_var)), arith_op);

  return new_comma_node(tmp_assign, assign);
}

static Node* assign(Token** tokens) {
  Node* node = conditional(tokens);

  for (;;) {
    Token* start = *tokens;

    if (consume_token(tokens, "=")) {
      node = new_assign_node(node, orr(tokens));
      continue;
    }

    if (consume_token(tokens, "|=")) {
      node = convert_to_assign_node(start, ND_BITOR, node, orr(tokens));
      continue;
    }

    if (consume_token(tokens, "^=")) {
      node = convert_to_assign_node(start, ND_BITXOR, node, orr(tokens));
      continue;
    }

    if (consume_token(tokens, "&=")) {
      node = convert_to_assign_node(start, ND_BITAND, node, orr(tokens));
      continue;
    }

    if (consume_token(tokens, "<<=")) {
      node = convert_to_assign_node(start, ND_LSHIFT, node, orr(tokens));
      continue;
    }

    if (consume_token(tokens, ">>=")) {
      node = convert_to_assign_node(start, ND_RSHIFT, node, orr(tokens));
      continue;
    }

    if (consume_token(tokens, "+=")) {
      node = convert_to_assign_node(start, ND_ADD, node, orr(tokens));
      continue;
    }

    if (consume_token(tokens, "-=")) {
      node = convert_to_assign_node(start, ND_SUB, node, orr(tokens));
      continue;
    }

    if (consume_token(tokens, "*=")) {
      node = convert_to_assign_node(start, ND_MUL, node, orr(tokens));
      continue;
    }

    if (consume_token(tokens, "/=")) {
      node = convert_to_assign_node(start, ND_DIV, node, orr(tokens));
      continue;
    }

    if (consume_token(tokens, "%=")) {
      node = convert_to_assign_node(start, ND_MOD, node, orr(tokens));
      continue;
    }

    return node;
  }
}

static Node* conditional(Token** tokens) {
  Token* start = *tokens;

  Node* node = orr(tokens);

  if (consume_token(tokens, "?")) {
    Node* then = expr(tokens);
    expect_token(tokens, ":");
    Node* els = conditional(tokens);

    return new_cond_node(start, node, then, els);
  }

  return node;
}

static Node* orr(Token** tokens) {
  Node* node = andd(tokens);
  while (consume_token(tokens, "||")) {
    node = new_or_node(node, andd(tokens));
  }

  return node;
}

static Node* andd(Token** tokens) {
  Node* node = bitorr(tokens);
  while (consume_token(tokens, "&&")) {
    node = new_and_node(node, bitorr(tokens));
  }

  return node;
}

static Node* bitorr(Token** tokens) {
  Node* node = bitxorr(tokens);
  while (consume_token(tokens, "|")) {
    node = new_bitor_node(node, bitxorr(tokens));
  }

  return node;
}

static Node* bitxorr(Token** tokens) {
  Node* node = bitandd(tokens);
  while (consume_token(tokens, "^")) {
    node = new_bitxor_node(node, bitandd(tokens));
  }

  return node;
}

static Node* bitandd(Token** tokens) {
  Node* node = equality(tokens);
  while (consume_token(tokens, "&")) {
    node = new_bitand_node(node, equality(tokens));
  }

  return node;
}

static Node* equality(Token** tokens) {
  Node* node = relational(tokens);

  for (;;) {
    if (consume_token(tokens, "==")) {
      node = new_eq_node(node, relational(tokens));
      continue;
    }

    if (consume_token(tokens, "!=")) {
      node = new_ne_node(node, relational(tokens));
      continue;
    }

    return node;
  }
}

static Node* relational(Token** tokens) {
  Node* node = shift(tokens);

  for (;;) {
    if (consume_token(tokens, "<")) {
      node = new_lt_node(node, shift(tokens));
      continue;
    }

    if (consume_token(tokens, "<=")) {
      node = new_le_node(node, shift(tokens));
      continue;
    }

    if (consume_token(tokens, ">")) {
      node = new_lt_node(shift(tokens), node);
      continue;
    }

    if (consume_token(tokens, ">=")) {
      node = new_le_node(shift(tokens), node);
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
    if (consume_token(tokens, "+")) {
      node = new_add_node(*tokens, node, mul(tokens));
      continue;
    }

    if (consume_token(tokens, "-")) {
      node = new_sub_node(*tokens, node, mul(tokens));
      continue;
    }

    return node;
  }
}

static Node* mul(Token** tokens) {
  Node* node = cast(tokens);

  for (;;) {
    if (consume_token(tokens, "*")) {
      node = new_mul_node(node, cast(tokens));
      continue;
    }

    if (consume_token(tokens, "/")) {
      node = new_div_node(node, cast(tokens));
      continue;
    }

    if (consume_token(tokens, "%")) {
      node = new_mod_node(node, cast(tokens));
      continue;
    }

    return node;
  }
}

static Node* cast(Token** tokens) {
  if (equal_to_token(*tokens, "(") && equal_to_type_name((*tokens)->next)) {
    Token* start = *tokens;
    expect_token(tokens, "(");
    Decl* decl = abstract_declarator(tokens, decl_specifier(tokens));
    expect_token(tokens, ")");
    return new_cast_node(decl->type, start, cast(tokens));
  }

  return unary(tokens);
}

static Node* unary(Token** tokens) {
  Token* start = *tokens;

  if (consume_token(tokens, "+")) {
    return cast(tokens);
  }

  if (consume_token(tokens, "-")) {
    return new_sub_node(start, new_long_node(start, 0), cast(tokens));
  }

  if (consume_token(tokens, "&")) {
    return new_addr_node(start, cast(tokens));
  }

  if (consume_token(tokens, "*")) {
    return new_deref_node(start, cast(tokens));
  }

  if (consume_token(tokens, "!")) {
    return new_not_node(start, cast(tokens));
  }

  if (consume_token(tokens, "~")) {
    return new_bitnot_node(start, cast(tokens));
  }

  if (consume_token(tokens, "++")) {
    return convert_to_assign_node(start, ND_ADD, unary(tokens),
                                  new_int_node(start, 1));
  }

  if (consume_token(tokens, "--")) {
    return convert_to_assign_node(start, ND_SUB, unary(tokens),
                                  new_int_node(start, 1));
  }

  if (consume_token(tokens, "sizeof")) {
    if (equal_to_token(*tokens, "(") && equal_to_type_name((*tokens)->next)) {
      expect_token(tokens, "(");
      Decl* decl = abstract_declarator(tokens, decl_specifier(tokens));
      expect_token(tokens, ")");
      return new_int_node(start, decl->type->size);
    }

    Node* node = cast(tokens);
    return new_int_node(start, node->type->size);
  }

  return postfix(tokens);
}

static Node* increment(Node* node, int delta) {
  Node* inc = convert_to_assign_node(node->token, ND_ADD, node,
                                     new_int_node(node->token, delta));
  return new_cast_node(
      node->type, node->token,
      new_add_node(node->token, inc, new_int_node(node->token, -delta)));
}

static Node* postfix(Token** tokens) {
  Node* node = primary(tokens);

  for (;;) {
    Token* start = *tokens;

    if (consume_token(tokens, "[")) {
      Node* index = expr(tokens);
      expect_token(tokens, "]");
      node = new_deref_node(start, new_add_node(*tokens, node, index));
      continue;
    }

    if (consume_token(tokens, ".")) {
      node = new_member_node(tokens, start, node);
      continue;
    }

    if (consume_token(tokens, "->")) {
      node = new_deref_node(start, node);
      node = new_member_node(tokens, start, node);
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

static Node* primary(Token** tokens) {
  Token* start = *tokens;

  if (consume_token(tokens, "(")) {
    if (equal_to_token(*tokens, "{")) {
      Node* node = new_stmt_expr_node(start, block_stmt(tokens)->body);
      expect_token(tokens, ")");
      return node;
    }

    Node* node = expr(tokens);
    expect_token(tokens, ")");
    return node;
  }

  if ((*tokens)->kind == TK_IDENT) {
    Token* ident = expect_ident(tokens);

    if (equal_to_token(*tokens, "(")) {
      Obj* func = find_func(ident->loc, ident->len);
      if (!func) {
        error_token(ident, "implicit declaration of a function");
      }

      return new_funccall_node(func->type, ident, func->name,
                               func_args(tokens, func->params));
    }

    Obj* var_or_enum = find_var_or_enum(ident->loc, ident->len);
    if (!var_or_enum) {
      error_token(ident, "undefined ident");
    }

    Node* node = var_or_enum->kind == OJ_ENUM
                     ? new_int_node(ident, var_or_enum->int_val)
                     : new_var_node(ident, var_or_enum);
    return node;
  }

  if ((*tokens)->kind == TK_NUM) {
    Node* node = new_int_node(*tokens, (*tokens)->int_val);
    *tokens = (*tokens)->next;
    return node;
  }

  if ((*tokens)->kind == TK_STR) {
    char* name = new_id();
    Obj* str = new_str(name, (*tokens)->str_val);
    Node* node = new_var_node(*tokens, str);
    *tokens = (*tokens)->next;
    return node;
  }

  error_token(*tokens, "expected a primary");
  return NULL;
}

static Node* lvar(Token** tokens) {
  Node* node = var_decl(tokens);
  expect_token(tokens, ";");
  return node;
}

static Node* var_decl(Token** tokens) {
  Token* start = *tokens;

  Type* spec = decl_specifier(tokens);

  Node head = {};
  Node* cur = &head;
  while (!equal_to_token(*tokens, ";")) {
    if (cur != &head) {
      expect_token(tokens, ",");
    }

    Decl* decl = declarator(tokens, spec);
    if (decl->type->size < 0) {
      error_token(decl->ident, "variable has imcomplete type");
    }

    Obj* var = new_lvar(decl->type, decl->name);
    Node* node = new_var_node(decl->ident, var);

    if (consume_token(tokens, "=")) {
      node = new_assign_node(node, assign(tokens));
    }

    cur->next = node;
    cur = cur->next;
  }

  Node* node = new_node(ND_BLOCK);
  node->token = start;
  node->body = head.next;
  return node;
}

static Type* enum_specifier(Token** tokens) {
  expect_token(tokens, "enum");

  Token* tag = NULL;
  if ((*tokens)->kind == TK_IDENT) {
    tag = expect_ident(tokens);
  }

  if (tag && !equal_to_token(*tokens, "{")) {
    Obj* found_tag = find_tag(tag->loc, tag->len);
    if (!found_tag) {
      error_token(tag, "undefined tag");
    }
    return found_tag->type;
  }

  expect_token(tokens, "{");

  bool is_first = true;
  int val = 0;
  while (!consume_token(tokens, "}")) {
    if (!is_first) {
      expect_token(tokens, ",");
    }
    is_first = false;

    Token* ident = expect_ident(tokens);

    if (consume_token(tokens, "=")) {
      val = expect_num(tokens);
    }

    new_enum(strndup(ident->loc, ident->len), val++);
  }

  Type* enu = ty_int;

  if (tag) {
    new_tag(enu, strndup(tag->loc, tag->len));
  }

  return enu;
}

static Type* struct_decl(Token** tokens) {
  expect_token(tokens, "struct");

  Token* tag = NULL;
  if ((*tokens)->kind == TK_IDENT) {
    tag = expect_ident(tokens);
  }

  if (tag && !equal_to_token(*tokens, "{")) {
    Obj* found = find_tag(tag->loc, tag->len);
    if (found) {
      return found->type;
    }

    Type* struc = new_struct_type(-1, 1, NULL);
    new_tag(struc, strndup(tag->loc, tag->len));
    return struc;
  }

  expect_token(tokens, "{");

  Member* mems = members(tokens);
  int offset = 0;
  int alignment = 1;
  for (Member* mem = mems; mem; mem = mem->next) {
    offset = align(offset, mem->type->alignment);
    mem->offset = offset;
    offset += mem->type->size;

    if (alignment < mem->type->alignment) {
      alignment = mem->type->alignment;
    }
  }

  Type* struc = new_struct_type(offset, alignment, mems);

  if (!tag) {
    return struc;
  }

  Obj* found = find_tag_in_current_scope(tag->loc, tag->len);
  if (found) {
    *found->type = *struc;
    return found->type;
  }

  new_tag(struc, strndup(tag->loc, tag->len));
  return struc;
}

static Type* union_decl(Token** tokens) {
  expect_token(tokens, "union");

  Token* tag = NULL;
  if ((*tokens)->kind == TK_IDENT) {
    tag = expect_ident(tokens);
  }

  if (tag && !equal_to_token(*tokens, "{")) {
    Obj* found = find_tag(tag->loc, tag->len);
    if (found) {
      return found->type;
    }

    Type* uni = new_union_type(-1, 1, NULL);
    new_tag(uni, strndup(tag->loc, tag->len));
    return uni;
  }

  expect_token(tokens, "{");

  Member* mems = members(tokens);
  int size = 0;
  int alignment = 1;
  for (Member* mem = mems; mem; mem = mem->next) {
    mem->offset = 0;
    if (size < mem->type->size) {
      size = mem->type->size;
    }
    if (alignment < mem->type->alignment) {
      alignment = mem->type->alignment;
    }
  }

  Type* uni = new_union_type(size, alignment, mems);

  if (!tag) {
    return uni;
  }

  Obj* found = find_tag_in_current_scope(tag->loc, tag->len);
  if (found) {
    *found->type = *uni;
    return found->type;
  }

  new_tag(uni, strndup(tag->loc, tag->len));
  return uni;
}

static Member* members(Token** tokens) {
  Member head = {};
  Member* cur = &head;
  while (!consume_token(tokens, "}")) {
    Type* spec = decl_specifier(tokens);

    bool is_first = true;
    while (!consume_token(tokens, ";")) {
      if (!is_first) {
        expect_token(tokens, ",");
      }
      is_first = false;

      Decl* decl = declarator(tokens, spec);
      if (decl->type->size < 0) {
        error_token(decl->ident, "variable has imcomplete type");
      }

      Member* mem = calloc(1, sizeof(Member));
      mem->type = decl->type;
      mem->name = decl->name;

      cur->next = mem;
      cur = cur->next;
    }
  }

  return head.next;
}

static Type* decl_specifier(Token** tokens) {
  Type* ty = ty_int;
  int counter = 0;

  enum {
    VOID = 1 << 0,
    BOOL = 1 << 2,
    CHAR = 1 << 4,
    SHORT = 1 << 6,
    INT = 1 << 8,
    LONG = 1 << 10,
    OTHER = 1 << 12,
  };

  while (equal_to_type_name(*tokens)) {
    Token* start = *tokens;

    Obj* def_type = find_def_type((*tokens)->loc, (*tokens)->len);
    if (def_type) {
      if (counter > 0) {
        break;
      }

      counter += OTHER;
      ty = def_type->type;
      *tokens = (*tokens)->next;
      continue;
    }

    if (equal_to_token(*tokens, "struct")) {
      if (counter > 0) {
        break;
      }

      counter += OTHER;
      ty = struct_decl(tokens);
      continue;
    }

    if (equal_to_token(*tokens, "union")) {
      if (counter > 0) {
        break;
      }

      counter += OTHER;
      ty = union_decl(tokens);
      continue;
    }

    if (equal_to_token(*tokens, "enum")) {
      if (counter > 0) {
        break;
      }

      counter += OTHER;
      ty = enum_specifier(tokens);
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

    switch (counter) {
      case VOID:
        ty = ty_void;
        break;
      case BOOL:
        ty = ty_bool;
        break;
      case CHAR:
        ty = ty_char;
        break;
      case SHORT:
      case SHORT + INT:
        ty = ty_short;
        break;
      case INT:
        ty = ty_int;
        break;
      case LONG:
      case LONG + INT:
      case LONG + LONG:
      case LONG + LONG + INT:
        ty = ty_long;
        break;
      default:
        error_token(start, "expected a typename");
    }
  }

  return ty;
}

static Decl* declarator(Token** tokens, Type* type) {
  while (consume_token(tokens, "*")) {
    type = new_ptr_type(type);
  }

  if (consume_token(tokens, "(")) {
    Token* start = *tokens;
    Type ignored = {};
    declarator(tokens, &ignored);
    expect_token(tokens, ")");
    type = type_suffix(tokens, type);
    return declarator(&start, type);
  }

  Token* ident = expect_ident(tokens);

  type = type_suffix(tokens, type);
  if (type == ty_void) {
    error_token(ident, "variable declared void");
  }

  Decl* decl = calloc(1, sizeof(Decl));
  decl->type = type;
  decl->ident = ident;
  decl->name = strndup(ident->loc, ident->len);
  return decl;
}

static Decl* abstract_declarator(Token** tokens, Type* type) {
  while (consume_token(tokens, "*")) {
    type = new_ptr_type(type);
  }

  if (consume_token(tokens, "(")) {
    Token* start = *tokens;
    Type ignored = {};
    abstract_declarator(tokens, &ignored);
    expect_token(tokens, ")");
    type = type_suffix(tokens, type);
    return abstract_declarator(&start, type);
  }

  Token* start = *tokens;

  type = type_suffix(tokens, type);
  if (type->size < 0) {
    error_token(start, "variable has imcomplete type");
  }

  Decl* decl = calloc(1, sizeof(Decl));
  decl->type = type;
  return decl;
}

static Type* type_suffix(Token** tokens, Type* type) {
  if (equal_to_token(*tokens, "[")) {
    return array_dimensions(tokens, type);
  }

  return type;
}

static Type* array_dimensions(Token** tokens, Type* type) {
  expect_token(tokens, "[");

  if (consume_token(tokens, "]")) {
    type = type_suffix(tokens, type);
    return new_array_type(type, -1);
  }

  int len = expect_num(tokens);
  expect_token(tokens, "]");
  type = type_suffix(tokens, type);
  return new_array_type(type, len);
}

static Node* func_args(Token** tokens, Node* params) {
  expect_token(tokens, "(");

  Node head = {};
  Node* cur = &head;
  while (!consume_token(tokens, ")")) {
    if (cur != &head) {
      expect_token(tokens, ",");
    }

    Node* arg = assign(tokens);

    if (params) {
      if (params->type->kind == TY_STRUCT || params->type->kind == TY_UNION) {
        error_token(arg->token, "passing struct or union is not supported yet");
      }

      if (is_numable(params->type)) {
        arg = new_cast_node(params->type, arg->token, arg);
      }

      params = params->next;
    }

    cur->next = arg;
    cur = cur->next;
  }

  return head.next;
}
