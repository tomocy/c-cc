#include "cc.h"

Obj* codes;

Scope* scope;
Scope* gscope;
Obj* lvars;

Type* ty_char = &(Type){
    TY_CHAR,
    1,
};
Type* ty_int = &(Type){
    TY_INT,
    8,
};

int str_count = 0;

static Type* new_type(TypeKind kind) {
  Type* type = calloc(1, sizeof(Type));
  type->kind = kind;
  return type;
}

static Type* add_size(Type* type) {
  switch (type->kind) {
    case TY_UNAVAILABLE:
      type->size = 0;
      break;
    case TY_CHAR:
      type->size = 1;
      break;
    case TY_INT:
    case TY_PTR:
      type->size = 8;
      break;
    case TY_ARRAY:
      type->size = type->base->size * type->len;
      break;
  }

  return type;
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

static void add_scoped_var_to(Scope* scope, Obj* var) {
  if (var->kind != OJ_GVAR && var->kind != OJ_LVAR) {
    error("expected a variable");
  }
  ScopedVar* scoped = calloc(1, sizeof(ScopedVar));
  scoped->var = var;
  scoped->next = scope->vars;
  scope->vars = scoped;
}

static bool is_gscope(Scope* scope) { return !scope->next; }

static void add_gvar(Obj* var) {
  if (var->kind != OJ_GVAR) {
    error("expected a global var");
  }
  if (!is_gscope(gscope)) {
    error("expected a global scope");
  }
  add_scoped_var_to(gscope, var);
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
  add_scoped_var_to(scope, var);
}

static Obj* find_func(char* name, int len) {
  for (Obj* func = codes; func; func = func->next) {
    if (func->kind != OJ_FUNC) {
      continue;
    }
    if (strlen(func->name) == len && strncmp(func->name, name, len) == 0) {
      return func;
    }
  }
  return NULL;
}

static Obj* find_var(char* name, int len) {
  for (Scope* s = scope; s; s = s->next) {
    for (ScopedVar* var = s->vars; var; var = var->next) {
      if (var->var->kind != OJ_GVAR && var->var->kind != OJ_LVAR) {
        continue;
      }
      if (strlen(var->var->name) == len &&
          strncmp(var->var->name, name, len) == 0) {
        return var->var;
      }
    }
  }
  return NULL;
}

static Obj* new_gvar(Type* type, char* name, int len) {
  Obj* var = new_obj(OJ_GVAR);
  var->type = type;
  var->name = strndup(name, len);
  add_gvar(var);
  return var;
}

static Obj* new_str(char* name, int name_len, char* data, int data_len) {
  Type* ty = new_type(TY_ARRAY);
  ty->base = ty_char;
  ty->len = data_len + 1;
  ty = add_size(ty);
  Obj* str = new_gvar(ty, name, name_len);
  str->data = strndup(data, data_len);
  add_code(str);
  return str;
}

static Obj* new_lvar(Type* type, char* name, int len) {
  Obj* var = new_obj(OJ_LVAR);
  var->type = type;
  var->name = strndup(name, len);
  var->offset = (lvars) ? lvars->offset + type->size : type->size;
  add_lvar(var);
  return var;
}

static Node* add_type(Node* node) {
  if (node->type) {
    return node;
  }

  switch (node->kind) {
    case ND_EQ:
    case ND_NE:
    case ND_LT:
    case ND_LE:
    case ND_NUM:
      node->type = ty_int;
      break;
    case ND_FUNCCALL: {
      Obj* func = find_func(node->name, strlen(node->name));
      node->type = (func) ? func->type : TY_UNAVAILABLE;
      break;
    }
    case ND_ASSIGN:
    case ND_ADD:
    case ND_SUB:
    case ND_MUL:
    case ND_DIV:
      node->type = node->lhs->type;
      break;
    case ND_ADDR:
      node->type = add_size(new_type(TY_PTR));
      node->type->base = node->lhs->type;
      break;
    case ND_DEREF:
      if (node->lhs->type->kind == TY_PTR ||
          node->lhs->type->kind == TY_ARRAY) {
        node->type = node->lhs->type->base;
      } else {
        node->type = node->lhs->type;
      }
      break;
    default:
      break;
  }
  return node;
}

static Node* new_node(NodeKind kind) {
  Node* node = calloc(1, sizeof(Node));
  node->kind = kind;
  return node;
}

static Node* new_unary_node(NodeKind kind, Node* lhs) {
  Node* node = new_node(kind);
  node->lhs = lhs;
  return add_type(node);
}

static Node* new_binary_node(NodeKind kind, Node* lhs, Node* rhs) {
  Node* node = new_node(kind);
  node->lhs = lhs;
  node->rhs = rhs;
  return add_type(node);
}

static Node* new_funccall_node(char* name, int len, Node* args) {
  Node* node = new_node(ND_FUNCCALL);
  node->name = strndup(name, len);
  node->args = args;
  return add_type(node);
}

static Node* new_num_node(int val) {
  Node* node = new_node(ND_NUM);
  node->val = val;
  return add_type(node);
}

static Node* new_gvar_node(Type* type, char* name, int len) {
  Node* node = new_node(ND_GVAR);
  node->type = type;
  node->name = strndup(name, len);
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
      return new_gvar_node(obj->type, obj->name, strlen(obj->name));
    case OJ_LVAR:
      return new_lvar_node(obj->type, obj->offset);
    default:
      error("expected a variable object");
      return NULL;
  }
}

static Node* expr();

static Node* func_args() {
  expect("(");
  Node head = {};
  Node* cur = &head;
  while (!consume(")")) {
    if (cur != &head) {
      expect(",");
    }
    cur->next = expr();
    cur = cur->next;
  }
  return head.next;
}

static Node* block_stmt();

static Node* primary() {
  if (consume("(")) {
    if (equal(token, "{")) {
      Node* node = new_node(ND_STMT_EXPR);
      node->body = block_stmt();
      expect(")");
      return node;
    }

    Node* node = expr();
    expect(")");
    return node;
  }

  if (token->kind == TK_IDENT) {
    if (equal(token->next, "(")) {
      Token* ident = token;
      token = token->next;
      return new_funccall_node(ident->str, ident->len, func_args());
    }

    Obj* var = find_var(token->str, token->len);
    if (!var) {
      error_at(token->str, "undefined ident");
    }
    Node* node = new_var_node(var);
    token = token->next;
    return node;
  }

  if (token->kind == TK_NUM) {
    return new_num_node(expect_num());
  }

  if (token->kind != TK_STR) {
    error_at(token->str, "expected a string literal");
  }
  char* name = calloc(20, sizeof(char));
  int len = sprintf(name, ".Lstr%d", str_count++);
  Obj* str = new_str(name, len, token->str, token->len);
  token = token->next;
  return new_var_node(str);
}

bool is_numable(Node* node) {
  return node->type->kind == TY_INT || node->type->kind == TY_CHAR;
}

bool is_pointable(Node* node) {
  return node->type->kind == TY_PTR || node->type->kind == TY_ARRAY;
}

static Node* new_add_node(NodeKind kind, Node* lhs, Node* rhs) {
  if (kind != ND_ADD && kind != ND_SUB) {
    error("expected an add");
  }

  if (is_pointable(lhs) && is_pointable(rhs)) {
    error_at(token->str, "invalid operands");
  }
  if (is_numable(lhs) && is_numable(rhs)) {
    return new_binary_node(kind, lhs, rhs);
  } else if (is_pointable(lhs) && is_numable(rhs)) {
    rhs = new_binary_node(ND_MUL, rhs, new_num_node(lhs->type->base->size));
    return new_binary_node(kind, lhs, rhs);
  } else {
    lhs = new_binary_node(ND_MUL, lhs, new_num_node(rhs->type->base->size));
    return new_binary_node(kind, lhs, rhs);
  }
}

static Node* postfix() {
  Node* node = primary();
  if (!consume("[")) {
    return node;
  }
  Node* index = expr();
  expect("]");
  return new_unary_node(ND_DEREF, new_add_node(ND_ADD, node, index));
}

static Node* unary() {
  if (consume("+")) {
    return primary();
  } else if (consume("-")) {
    return new_binary_node(ND_SUB, new_num_node(0), primary());
  } else if (consume("&")) {
    return new_unary_node(ND_ADDR, unary());
  } else if (consume("*")) {
    return new_unary_node(ND_DEREF, unary());
  } else if (consume("sizeof")) {
    Node* node = unary();
    return new_num_node(node->type->size);
  } else {
    return postfix();
  }
}

static Node* mul() {
  Node* node = unary();

  for (;;) {
    if (consume("*")) {
      node = new_binary_node(ND_MUL, node, unary());
    } else if (consume("/")) {
      node = new_binary_node(ND_DIV, node, unary());
    } else {
      return node;
    }
  }
}

static Node* add() {
  Node* node = mul();

  for (;;) {
    if (consume("+")) {
      Node* rhs = mul();
      node = new_add_node(ND_ADD, node, rhs);
    } else if (consume("-")) {
      Node* rhs = mul();
      node = new_add_node(ND_SUB, node, rhs);
    } else {
      return node;
    }
  }
}

static Node* relational() {
  Node* node = add();

  for (;;) {
    if (consume("<")) {
      node = new_binary_node(ND_LT, node, add());
    } else if (consume("<=")) {
      node = new_binary_node(ND_LE, node, add());
    } else if (consume(">")) {
      node = new_binary_node(ND_LT, add(), node);
    } else if (consume(">=")) {
      node = new_binary_node(ND_LE, add(), node);
    } else {
      return node;
    }
  }
}

static Node* equality() {
  Node* node = relational();

  for (;;) {
    if (consume("==")) {
      node = new_binary_node(ND_EQ, node, relational());
    } else if (consume("!=")) {
      node = new_binary_node(ND_NE, node, relational());
    } else {
      return node;
    }
  }
}

static Node* assign() {
  Node* node = equality();

  for (;;) {
    if (consume("=")) {
      node = new_binary_node(ND_ASSIGN, node, equality());
    } else {
      return node;
    }
  }
}

static Node* expr() { return assign(); }

static Type* type_head() {
  Type* cur;
  if (consume("char")) {
    cur = ty_char;
  } else {
    expect("int");
    cur = ty_int;
  }

  while (consume("*")) {
    Type* head = add_size(new_type(TY_PTR));
    head->base = cur;
    cur = head;
  }
  return cur;
}

static Type* type_tail(Type* head) {
  expect("[");
  int len = expect_num();
  expect("]");

  Type* array = new_type(TY_ARRAY);
  array->len = len;
  array->base = head;
  return add_size(array);
}

static Node* lvar() {
  Type* ty = type_head();

  if (token->kind != TK_IDENT) {
    error_at(token->str, "expected an ident");
  }
  Token* ident = token;
  token = token->next;

  if (equal(token, "[")) {
    ty = type_tail(ty);
  }

  Obj* var = new_lvar(ty, ident->str, ident->len);
  Node* node = new_var_node(var);
  if (!consume("=")) {
    return node;
  }
  return new_binary_node(ND_ASSIGN, node, expr());
}

bool equal_type_name(Token* tok) {
  static char* tnames[] = {"int", "char"};
  int len = sizeof(tnames) / sizeof(char*);
  for (int i = 0; i < len; i++) {
    if (equal(tok, tnames[i])) {
      return true;
    }
  }
  return false;
}

static Node* block_stmt();

static Node* stmt() {
  if (equal(token, "{")) {
    return block_stmt();
  } else if (consume("if")) {
    expect("(");
    Node* node = new_node(ND_IF);
    node->cond = expr();
    expect(")");
    node->then = stmt();
    if (consume("else")) {
      node->els = stmt();
    }
    return node;
  } else if (consume("while")) {
    expect("(");
    Node* node = new_node(ND_FOR);
    node->cond = expr();
    expect(")");
    node->then = stmt();
    return node;
  } else if (consume("for")) {
    expect("(");
    Node* node = new_node(ND_FOR);
    if (!consume(";")) {
      node->init = expr();
      expect(";");
    }
    if (!consume(";")) {
      node->cond = expr();
      expect(";");
    }
    if (!consume(")")) {
      node->inc = expr();
      expect(")");
    }
    node->then = stmt();
    return node;
  } else if (consume("return")) {
    Node* node = new_unary_node(ND_RETURN, expr());
    expect(";");
    return node;
  } else if (equal_type_name(token)) {
    Node* node = lvar();
    expect(";");
    return node;
  } else {
    Node* node = expr();
    expect(";");
    return node;
  }
}

static Node* block_stmt() {
  expect("{");
  enter_scope();
  Node head = {};
  Node* curr = &head;
  while (!consume("}")) {
    curr->next = stmt();
    curr = curr->next;
  }
  leave_scope();

  Node* node = new_node(ND_BLOCK);
  node->body = head.next;
  return node;
}

static void gvar() {
  Type* ty = type_head();

  if (token->kind != TK_IDENT) {
    error_at(token->str, "expected an ident");
  }
  Token* ident = token;
  token = token->next;

  if (equal(token, "[")) {
    ty = type_tail(ty);
  }

  Obj* var = new_gvar(ty, ident->str, ident->len);
  add_code(var);
  if (consume("=")) {
    var->val = expect_num();
  }
}

static void func() {
  Type* ty = type_head();

  if (token->kind != TK_IDENT) {
    error_at(token->str, "expected an ident");
  }
  Obj* func = new_obj(OJ_FUNC);
  add_code(func);
  func->type = ty;
  func->name = strndup(token->str, token->len);
  token = token->next;

  expect("(");
  enter_scope();
  Node head = {};
  Node* cur = &head;
  while (!consume(")")) {
    if (cur != &head) {
      expect(",");
    }

    Node* param = lvar();
    cur->next = param;
    cur = cur->next;
  }
  func->params = head.next;

  func->body = block_stmt();

  leave_scope();
  func->lvars = lvars;
  lvars = NULL;
}

void program() {
  codes = NULL;
  scope = NULL;
  gscope = NULL;
  lvars = NULL;

  enter_scope();
  gscope = scope;

  while (!at_eof()) {
    Token* start = token;

    type_head();
    if (token->kind != TK_IDENT) {
      error_at(token->str, "expected an ident");
    }
    token = token->next;
    bool is_func = equal(token, "(");
    token = start;

    if (is_func) {
      func();
    } else {
      gvar();
      expect(";");
    }
  }

  leave_scope();
}