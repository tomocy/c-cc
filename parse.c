#include "cc.h"

typedef struct ScopedObj ScopedObj;
typedef struct Scope Scope;

typedef struct Decl {
  Type* type;
  char* name;
} Decl;

struct ScopedObj {
  ScopedObj* next;
  Obj* obj;
};

struct Scope {
  Scope* next;
  ScopedObj* objs;
};

static Obj* codes;
static Scope* scope;
static Scope* gscope;
static Obj* lvars;

static Type* ty_unavailable = &(Type){
    TY_UNAVAILABLE,
    0,
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

int str_count = 0;

static int align(int n, int align) { return (n + align - 1) / align * align; }

bool strs_equal(char* a, char* b, int b_len) {
  return strlen(a) == b_len && strncmp(a, b, b_len) == 0;
}

static Type* new_type(TypeKind kind, int size, int alignment) {
  Type* type = calloc(1, sizeof(Type));
  type->kind = kind;
  type->size = size;
  type->alignment = alignment;
  return type;
}

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

static Obj* new_obj(ObjKind kind) {
  Obj* obj = calloc(1, sizeof(Obj));
  obj->kind = kind;
  return obj;
}

static void add_code(Obj* code) {
  if (code->kind != OJ_FUNC && code->kind != OJ_GVAR) {
    error("expected a top level object");
  }
  code->next = codes;
  codes = code;
}

static void enter_scope() {
  Scope* next = calloc(1, sizeof(Scope));
  next->next = scope;
  scope = next;
}

static void leave_scope() {
  if (!scope) {
    error("no scope to leave");
  }
  scope = scope->next;
}

static void add_obj_to_scope(Scope* scope, Obj* obj) {
  ScopedObj* scoped = calloc(1, sizeof(ScopedObj));
  scoped->obj = obj;
  scoped->next = scope->objs;
  scope->objs = scoped;
}

static void add_var_to_scope(Scope* scope, Obj* obj) {
  if (obj->kind != OJ_GVAR && obj->kind != OJ_LVAR) {
    error("expected a variable");
  }
  add_obj_to_scope(scope, obj);
}

static void add_tag_to_scope(Scope* scope, Obj* obj) {
  if (obj->kind != OJ_TAG) {
    error("expected a tag");
  }
  add_obj_to_scope(scope, obj);
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
  if (is_gscope(scope)) {
    error("expected a local scope");
  }
  var->next = lvars;
  lvars = var;
  add_var_to_scope(scope, var);
}

static void add_tag(Obj* tag) {
  if (tag->kind != OJ_TAG) {
    error("expected a tag");
  }
  add_tag_to_scope(scope, tag);
}

static Obj* find_func(char* name, int len) {
  for (Obj* func = codes; func; func = func->next) {
    if (func->kind != OJ_FUNC) {
      continue;
    }
    if (strs_equal(func->name, name, len)) {
      return func;
    }
  }
  return NULL;
}

static Obj* find_var(char* name, int len) {
  for (Scope* s = scope; s; s = s->next) {
    for (ScopedObj* var = s->objs; var; var = var->next) {
      if (var->obj->kind != OJ_GVAR && var->obj->kind != OJ_LVAR) {
        continue;
      }
      if (strs_equal(var->obj->name, name, len)) {
        return var->obj;
      }
    }
  }
  return NULL;
}

static Obj* find_tag(char* name, int len) {
  for (Scope* s = scope; s; s = s->next) {
    for (ScopedObj* tag = s->objs; tag; tag = tag->next) {
      if (tag->obj->kind != OJ_TAG) {
        continue;
      }
      if (strs_equal(tag->obj->name, name, len)) {
        return tag->obj;
      }
    }
  }
  return NULL;
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
      align((lvars) ? lvars->offset + type->size : type->size, type->alignment);
  add_lvar(var);
  return var;
}

static Obj* new_tag(Type* type, char* name) {
  Obj* tag = new_obj(OJ_TAG);
  tag->type = type;
  tag->name = name;
  add_tag(tag);
  return tag;
}

static Node* new_node(NodeKind kind) {
  Node* node = calloc(1, sizeof(Node));
  node->kind = kind;
  return node;
}

static Node* new_unary_node(NodeKind kind, Node* lhs) {
  Node* node = new_node(kind);
  node->lhs = lhs;
  return node;
}

static Node* new_binary_node(NodeKind kind, Node* lhs, Node* rhs) {
  Node* node = new_node(kind);
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

static Node* new_member_node(Token** tokens, Node* lhs) {
  if (lhs->type->kind != TY_STRUCT && lhs->type->kind != TY_UNION) {
    error_token(*tokens, "expected a struct or union");
  }
  if ((*tokens)->kind != TK_IDENT) {
    error_token(*tokens, "expected an ident");
  }

  for (Member* mem = lhs->type->members; mem; mem = mem->next) {
    if (!strs_equal(mem->name, (*tokens)->loc, (*tokens)->len)) {
      continue;
    }

    *tokens = (*tokens)->next;
    Node* node = new_unary_node(ND_MEMBER, lhs);
    node->type = mem->type;
    node->name = mem->name;
    node->offset = mem->offset;
    return node;
  }

  error_token(*tokens, "no such member");
  return NULL;
}

static Node* new_gvar_node(Type* type, char* name) {
  Node* node = new_node(ND_GVAR);
  node->type = type;
  node->name = name;
  return node;
}

static Node* new_lvar_node(Type* type, int offset) {
  Node* node = new_node(ND_LVAR);
  node->type = type;
  node->offset = offset;
  return node;
}

static Node* new_var_node(Obj* obj) {
  switch (obj->kind) {
    case OJ_GVAR:
      return new_gvar_node(obj->type, obj->name);
    case OJ_LVAR:
      return new_lvar_node(obj->type, obj->offset);
    default:
      error("expected a variable object");
      return NULL;
  }
}

static Node* new_funccall_node(char* name, Node* args) {
  Node* node = new_node(ND_FUNCCALL);
  node->name = name;
  node->args = args;
  Obj* func = find_func(name, strlen(node->name));
  node->type = (func) ? func->type : ty_unavailable;
  return node;
}

static Node* new_num_node(int64_t val) {
  Node* node = new_node(ND_NUM);
  node->val = val;
  node->type = ty_long;
  return node;
}

static Node* new_mul_node(Node* lhs, Node* rhs) {
  Node* mul = new_binary_node(ND_MUL, lhs, rhs);
  mul->type = lhs->type;
  return mul;
}

static Node* new_div_node(Node* lhs, Node* rhs) {
  Node* div = new_binary_node(ND_DIV, lhs, rhs);
  div->type = lhs->type;
  return div;
}

bool is_numable(Node* node) {
  return node->type->kind == TY_CHAR || node->type->kind == TY_SHORT ||
         node->type->kind == TY_INT || node->type->kind == TY_LONG;
}

bool is_pointable(Node* node) {
  return node->type->kind == TY_PTR || node->type->kind == TY_ARRAY;
}

static Node* new_add_node(Token* tokens, Node* lhs, Node* rhs) {
  if (is_pointable(lhs) && is_pointable(rhs)) {
    error_token(tokens, "invalid operands");
  }

  Node* add;
  if (is_numable(lhs) && is_numable(rhs)) {
    add = new_binary_node(ND_ADD, lhs, rhs);
  } else if (is_pointable(lhs) && is_numable(rhs)) {
    rhs = new_mul_node(rhs, new_num_node(lhs->type->base->size));
    add = new_binary_node(ND_ADD, lhs, rhs);
  } else {
    lhs = new_mul_node(lhs, new_num_node(rhs->type->base->size));
    add = new_binary_node(ND_ADD, lhs, rhs);
  }
  add->type = add->lhs->type;
  return add;
}

static Node* new_sub_node(Node* lhs, Node* rhs) {
  Node* sub;
  if (is_pointable(lhs) && is_pointable(rhs)) {
    sub = new_binary_node(ND_SUB, lhs, rhs);
    sub->type = sub->lhs->type;
    return new_div_node(sub, new_num_node(sub->lhs->type->base->size));
  }

  if (is_numable(lhs) && is_numable(rhs)) {
    sub = new_binary_node(ND_SUB, lhs, rhs);
  } else if (is_pointable(lhs) && is_numable(rhs)) {
    rhs = new_mul_node(rhs, new_num_node(lhs->type->base->size));
    sub = new_binary_node(ND_SUB, lhs, rhs);
  } else {
    lhs = new_mul_node(lhs, new_num_node(rhs->type->base->size));
    sub = new_binary_node(ND_SUB, lhs, rhs);
  }
  sub->type = sub->lhs->type;
  return sub;
}

static Node* new_addr_node(Node* lhs) {
  Node* addr = new_unary_node(ND_ADDR, lhs);
  addr->type = new_ptr_type(addr->lhs->type);
  return addr;
}

static Node* new_deref_node(Node* lhs) {
  Node* deref = new_unary_node(ND_DEREF, lhs);
  deref->type =
      (deref->lhs->type->base) ? deref->lhs->type->base : deref->lhs->type;
  return deref;
}

static Node* new_eq_node(Node* lhs, Node* rhs) {
  Node* eq = new_binary_node(ND_EQ, lhs, rhs);
  eq->type = ty_long;
  return eq;
}

static Node* new_ne_node(Node* lhs, Node* rhs) {
  Node* ne = new_binary_node(ND_NE, lhs, rhs);
  ne->type = ty_long;
  return ne;
}

static Node* new_lt_node(Node* lhs, Node* rhs) {
  Node* lt = new_binary_node(ND_LT, lhs, rhs);
  lt->type = ty_long;
  return lt;
}

static Node* new_le_node(Node* lhs, Node* rhs) {
  Node* le = new_binary_node(ND_LE, lhs, rhs);
  le->type = ty_long;
  return le;
}

static Node* new_assign_node(Node* lhs, Node* rhs) {
  Node* assign = new_binary_node(ND_ASSIGN, lhs, rhs);
  assign->type = lhs->type;
  return assign;
}

static Node* new_comma_node(Node* lhs, Node* rhs) {
  Node* comma = new_binary_node(ND_COMMA, lhs, rhs);
  comma->type = comma->rhs->type;
  return comma;
}

static Node* assign();
static Node* block_stmt();
static Node* expr();
static Type* struct_decl();
static Type* union_decl();
static Node* block_stmt();

static Node* func_args(Token** tokens) {
  expect_token(tokens, "(");
  Node head = {};
  Node* cur = &head;
  while (!consume_token(tokens, ")")) {
    if (cur != &head) {
      expect_token(tokens, ",");
    }
    cur->next = assign(tokens);
    cur = cur->next;
  }
  return head.next;
}

static Node* primary(Token** tokens) {
  if (consume_token(tokens, "(")) {
    if (equal_to_token(*tokens, "{")) {
      Node* node = new_node(ND_STMT_EXPR);
      node->body = block_stmt(tokens);
      expect_token(tokens, ")");
      return node;
    }

    Node* node = expr(tokens);
    expect_token(tokens, ")");
    return node;
  }

  if ((*tokens)->kind == TK_IDENT) {
    if (equal_to_token((*tokens)->next, "(")) {
      Token* ident = *tokens;
      *tokens = (*tokens)->next;
      return new_funccall_node(strndup(ident->loc, ident->len),
                               func_args(tokens));
    }

    Obj* var = find_var((*tokens)->loc, (*tokens)->len);
    if (!var) {
      error_token(*tokens, "undefined ident");
    }
    Node* node = new_var_node(var);
    *tokens = (*tokens)->next;
    return node;
  }

  if ((*tokens)->kind == TK_NUM) {
    return new_num_node(expect_num(tokens));
  }

  if ((*tokens)->kind == TK_STR) {
    char* name = calloc(20, sizeof(char));
    sprintf(name, ".Lstr%d", str_count++);
    Obj* str = new_str(name, (*tokens)->str_val);
    *tokens = (*tokens)->next;
    return new_var_node(str);
  }

  error_token(*tokens, "expected a primary");
  return NULL;
}

static Node* postfix(Token** tokens) {
  Node* node = primary(tokens);

  for (;;) {
    if (consume_token(tokens, "[")) {
      Node* index = expr(tokens);
      expect_token(tokens, "]");
      node = new_deref_node(new_add_node(*tokens, node, index));
      continue;
    }

    if (consume_token(tokens, ".")) {
      node = new_member_node(tokens, node);
      continue;
    }

    if (consume_token(tokens, "->")) {
      node = new_deref_node(node);
      node = new_member_node(tokens, node);
      continue;
    }

    return node;
  }
}

static Node* unary(Token** tokens) {
  if (consume_token(tokens, "+")) {
    return primary(tokens);
  }

  if (consume_token(tokens, "-")) {
    return new_sub_node(new_num_node(0), primary(tokens));
  }

  if (consume_token(tokens, "&")) {
    return new_addr_node(unary(tokens));
  }

  if (consume_token(tokens, "*")) {
    return new_deref_node(unary(tokens));
  }

  if (consume_token(tokens, "sizeof")) {
    Node* node = unary(tokens);
    return new_num_node(node->type->size);
  }

  return postfix(tokens);
}

static Node* mul(Token** tokens) {
  Node* node = unary(tokens);

  for (;;) {
    if (consume_token(tokens, "*")) {
      node = new_mul_node(node, unary(tokens));
      continue;
    }

    if (consume_token(tokens, "/")) {
      node = new_div_node(node, unary(tokens));
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
      node = new_sub_node(node, mul(tokens));
      continue;
    }

    return node;
  }
}

static Node* relational(Token** tokens) {
  Node* node = add(tokens);

  for (;;) {
    if (consume_token(tokens, "<")) {
      node = new_lt_node(node, add(tokens));
      continue;
    }

    if (consume_token(tokens, "<=")) {
      node = new_le_node(node, add(tokens));
      continue;
    }

    if (consume_token(tokens, ">")) {
      node = new_lt_node(add(tokens), node);
      continue;
    }

    if (consume_token(tokens, ">=")) {
      node = new_le_node(add(tokens), node);
      continue;
    }

    return node;
  }
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

static Node* assign(Token** tokens) {
  Node* node = equality(tokens);

  for (;;) {
    if (consume_token(tokens, "=")) {
      node = new_assign_node(node, equality(tokens));
      continue;
    }

    return node;
  }
}

static Node* expr(Token** tokens) {
  Node* node = assign(tokens);
  if (consume_token(tokens, ",")) {
    return new_comma_node(node, expr(tokens));
  }
  return node;
}

static Type* decl_specifier(Token** tokens) {
  if (consume_token(tokens, "char")) {
    return ty_char;
  }

  if (consume_token(tokens, "short")) {
    return ty_short;
  }

  if (consume_token(tokens, "int")) {
    return ty_int;
  }

  if (consume_token(tokens, "long")) {
    return ty_long;
  }

  if (equal_to_token(*tokens, "struct")) {
    return struct_decl(tokens);
  }

  if (equal_to_token(*tokens, "union")) {
    return union_decl(tokens);
  }

  error_token(*tokens, "expected a typename");
  return NULL;
}

static Type* type_suffix(Token** tokens, Type* ty) {
  if (consume_token(tokens, "[")) {
    int len = expect_num(tokens);
    expect_token(tokens, "]");
    ty = type_suffix(tokens, ty);
    return new_array_type(ty, len);
  }

  return ty;
}

static Decl* declarator(Token** tokens, Type* ty) {
  while (consume_token(tokens, "*")) {
    ty = new_ptr_type(ty);
  }

  if ((*tokens)->kind != TK_IDENT) {
    error_token(*tokens, "expected an ident");
  }
  Token* ident = *tokens;
  *tokens = (*tokens)->next;

  ty = type_suffix(tokens, ty);

  Decl* decl = calloc(1, sizeof(Decl));
  decl->type = ty;
  decl->name = strndup(ident->loc, ident->len);
  return decl;
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

      Member* mem = calloc(1, sizeof(Member));
      mem->type = decl->type;
      mem->name = decl->name;

      cur->next = mem;
      cur = cur->next;
    }
  }

  return head.next;
}

static Type* struct_decl(Token** tokens) {
  expect_token(tokens, "struct");

  Token* tag = NULL;
  if ((*tokens)->kind == TK_IDENT) {
    tag = *tokens;
    *tokens = (*tokens)->next;
  }

  if (tag && !equal_to_token(*tokens, "{")) {
    Obj* found_tag = find_tag(tag->loc, tag->len);
    if (!found_tag) {
      error_token(tag, "undefined tag");
    }
    return found_tag->type;
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

  if (tag) {
    new_tag(struc, strndup(tag->loc, tag->len));
  }

  return struc;
}

static Type* union_decl(Token** tokens) {
  expect_token(tokens, "union");

  Token* tag = NULL;
  if ((*tokens)->kind == TK_IDENT) {
    tag = *tokens;
    *tokens = (*tokens)->next;
  }

  if (tag && !equal_to_token(*tokens, "{")) {
    Obj* found_tag = find_tag(tag->loc, tag->len);
    if (!found_tag) {
      error_token(tag, "undefined tag");
    }
    return found_tag->type;
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

  if (tag) {
    new_tag(uni, strndup(tag->loc, tag->len));
  }

  return uni;
}

static Node* lvar(Token** tokens) {
  Type* spec = decl_specifier(tokens);

  Node head = {};
  Node* cur = &head;
  while (!consume_token(tokens, ";")) {
    if (cur != &head) {
      expect_token(tokens, ",");
    }

    Decl* decl = declarator(tokens, spec);
    Obj* var = new_lvar(decl->type, decl->name);
    Node* node = new_var_node(var);

    if (consume_token(tokens, "=")) {
      node = new_assign_node(node, assign(tokens));
    }

    cur->next = node;
    cur = cur->next;
  }

  Node* node = new_node(ND_BLOCK);
  node->body = head.next;
  return node;
}

bool equal_to_type_name(Token* token) {
  static char* names[] = {"char", "short", "int", "long", "struct", "union"};
  int len = sizeof(names) / sizeof(char*);
  for (int i = 0; i < len; i++) {
    if (equal_to_token(token, names[i])) {
      return true;
    }
  }
  return false;
}

static Node* stmt(Token** tokens) {
  if (equal_to_token(*tokens, "{")) {
    return block_stmt(tokens);
  }

  if (consume_token(tokens, "if")) {
    expect_token(tokens, "(");
    Node* node = new_node(ND_IF);
    node->cond = expr(tokens);
    expect_token(tokens, ")");
    node->then = stmt(tokens);
    if (consume_token(tokens, "else")) {
      node->els = stmt(tokens);
    }
    return node;
  }

  if (consume_token(tokens, "while")) {
    expect_token(tokens, "(");
    Node* node = new_node(ND_FOR);
    node->cond = expr(tokens);
    expect_token(tokens, ")");
    node->then = stmt(tokens);
    return node;
  }

  if (consume_token(tokens, "for")) {
    expect_token(tokens, "(");
    Node* node = new_node(ND_FOR);
    if (!consume_token(tokens, ";")) {
      node->init = expr(tokens);
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
    return node;
  }

  if (consume_token(tokens, "return")) {
    Node* node = new_unary_node(ND_RETURN, expr(tokens));
    expect_token(tokens, ";");
    return node;
  }

  if (equal_to_type_name(*tokens)) {
    return lvar(tokens);
  }

  Node* node = expr(tokens);
  expect_token(tokens, ";");
  return node;
}

static Node* block_stmt(Token** tokens) {
  expect_token(tokens, "{");
  enter_scope();

  Node head = {};
  Node* curr = &head;
  while (!consume_token(tokens, "}")) {
    curr->next = stmt(tokens);
    curr = curr->next;
  }

  leave_scope();

  Node* node = new_node(ND_BLOCK);
  node->body = head.next;
  return node;
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
    Obj* var = new_gvar(decl->type, decl->name);
    add_code(var);

    if (consume_token(tokens, "=")) {
      var->int_val = expect_num(tokens);
    }
  }
}

static void func(Token** tokens) {
  Type* ty = decl_specifier(tokens);

  if ((*tokens)->kind != TK_IDENT) {
    error_token(*tokens, "expected an ident");
  }

  Obj* func = new_obj(OJ_FUNC);
  add_code(func);
  func->type = ty;
  func->name = strndup((*tokens)->loc, (*tokens)->len);
  *tokens = (*tokens)->next;

  expect_token(tokens, "(");
  enter_scope();

  Node head = {};
  Node* cur = &head;
  while (!consume_token(tokens, ")")) {
    if (cur != &head) {
      expect_token(tokens, ",");
    }

    Type* spec = decl_specifier(tokens);
    Decl* decl = declarator(tokens, spec);

    Obj* var = new_lvar(decl->type, decl->name);
    Node* param = new_var_node(var);
    cur->next = param;
    cur = cur->next;
  }
  func->params = head.next;

  func->body = block_stmt(tokens);

  leave_scope();

  func->lvars = lvars;
  func->stack_size = align((func->lvars) ? func->lvars->offset : 0, 16);
  lvars = NULL;
}

bool is_func(Token* tokens) {
  decl_specifier(&tokens);

  if (tokens->kind != TK_IDENT) {
    error_token(tokens, "expected an ident");
  }
  tokens = tokens->next;
  return equal_to_token(tokens, "(");
}

Obj* parse(Token* tokens) {
  enter_scope();
  gscope = scope;

  while (tokens->kind != TK_EOF) {
    if (is_func(tokens)) {
      func(&tokens);
    } else {
      gvar(&tokens);
    }
  }

  leave_scope();

  return codes;
}