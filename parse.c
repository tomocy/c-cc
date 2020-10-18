#include "cc.h"

Obj* codes;

Obj* gvars;
Obj* lvars;

int str_count = 0;

void add_code(Obj* code) {
  if (code->kind != OJ_FUNC && code->kind != OJ_GVAR) {
    error("expected a top level object");
  }
  code->next = codes;
  codes = code;
}

void add_gvar(Obj* var) {
  if (var->kind != OJ_GVAR) {
    error("expected a global var");
  }
  var->next = gvars;
  gvars = var;
}

void add_lvar(Obj* var) {
  if (var->kind != OJ_LVAR) {
    error("expected a local var");
  }
  var->next = lvars;
  lvars = var;
}

Obj* new_obj(ObjKind kind) {
  Obj* obj = calloc(1, sizeof(Obj));
  obj->kind = kind;
  return obj;
}

Type* new_type(TypeKind kind) {
  Type* type = calloc(1, sizeof(Type));
  type->kind = kind;
  return type;
}

Type* add_size(Type* type) {
  switch (type->kind) {
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

Node* add_type(Node* node) {
  if (node->type) {
    return node;
  }

  switch (node->kind) {
    case ND_EQ:
    case ND_NE:
    case ND_LT:
    case ND_LE:
    case ND_NUM:
      node->type = add_size(new_type(TY_INT));
      break;
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
      if (node->lhs->type->kind == TY_PTR) {
        node->type = node->lhs->type->base;
      } else if (node->lhs->type->kind == TY_ARRAY) {
        node->type = node->lhs->type->base;
      } else {
        node->type = node->lhs->type;
      }
    default:
      break;
  }
  return node;
}

Node* new_node(NodeKind kind) {
  Node* node = calloc(1, sizeof(Node));
  node->kind = kind;
  return node;
}

Node* new_unary_node(NodeKind kind, Node* lhs) {
  Node* node = new_node(kind);
  node->lhs = lhs;
  return add_type(node);
}

Node* new_binary_node(NodeKind kind, Node* lhs, Node* rhs) {
  Node* node = new_node(kind);
  node->lhs = lhs;
  node->rhs = rhs;
  return add_type(node);
}

Node* new_num_node(int val) {
  Node* node = new_node(ND_NUM);
  node->val = val;
  return add_type(node);
}

Obj* find_var_from(Obj* head, char* name, int len) {
  for (Obj* var = head; var; var = var->next) {
    if (var->kind != OJ_GVAR && var->kind != OJ_LVAR) {
      continue;
    }
    if (var->len == len && memcmp(var->name, name, len) == 0) {
      return var;
    }
  }
  return NULL;
}

Obj* find_var(char* name, int len) {
  Obj* var = find_var_from(lvars, name, len);
  if (var) {
    return var;
  }
  return find_var_from(gvars, name, len);
}

Obj* new_gvar(Type* type, char* name, int len) {
  Obj* var = new_obj(OJ_GVAR);
  var->type = type;
  var->name = name;
  var->len = len;
  add_gvar(var);
  return var;
}

Obj* new_str(char* name, int len, char* data, int data_len) {
  Type* ty = new_type(TY_ARRAY);
  ty->base = add_size(new_type(TY_CHAR));
  ty->len = data_len + 1;
  ty = add_size(ty);
  Obj* str = new_gvar(ty, name, len);
  str->data = malloc(ty->size);
  sprintf(str->data, "%.*s", data_len, data);
  add_code(str);
  return str;
}

Obj* new_lvar(Type* type, char* name, int len) {
  Obj* var = new_obj(OJ_LVAR);
  var->type = type;
  var->name = name;
  var->len = len;
  var->offset = (lvars) ? lvars->offset + type->size : type->size;
  add_lvar(var);
  return var;
}

Obj* find_or_new_lvar(Type* type, char* name, int len) {
  Obj* var = find_var_from(lvars, name, len);
  if (var) {
    return var;
  }
  return new_lvar(type, name, len);
}

Node* new_gvar_node(Type* type, char* name, int len) {
  Node* node = new_node(ND_GVAR);
  node->type = type;
  node->name = name;
  node->len = len;
  return node;
}

Node* new_lvar_node(Type* type, int offset) {
  Node* node = new_node(ND_LVAR);
  node->type = type;
  node->offset = offset;
  return node;
}

Node* new_var_node(Obj* obj) {
  switch (obj->kind) {
    case OJ_GVAR:
      return new_gvar_node(obj->type, obj->name, obj->len);
    case OJ_LVAR:
      return new_lvar_node(obj->type, obj->offset);
    default:
      error("expected a variable object");
      return NULL;
  }
}

Node* expr();

Node* func_args() {
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

Node* primary() {
  if (consume("(")) {
    Node* node = expr();
    expect(")");
    return node;
  }

  if (token->kind == TK_IDENT) {
    if (equal(token->next, "(")) {
      Node* node = new_node(ND_FUNCCALL);
      node->name = token->str;
      node->len = token->len;
      token = token->next;
      node->args = func_args();
      return node;
    }

    Obj* var = find_var(token->str, token->len);
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

bool is_num(Node* node) {
  return node->type->kind == TY_INT || node->type->kind == TY_CHAR;
}

bool is_pointable(Node* node) {
  return node->type->kind == TY_PTR || node->type->kind == TY_ARRAY;
}

Node* new_add_node(NodeKind kind, Node* lhs, Node* rhs) {
  if (kind != ND_ADD && kind != ND_SUB) {
    error("expected an add");
  }

  if (is_pointable(lhs) && is_pointable(rhs)) {
    error_at(token->str, "invalid operands");
  }
  if (is_num(lhs) && is_num(rhs)) {
    return new_binary_node(kind, lhs, rhs);
  } else if (is_pointable(lhs) && is_num(rhs)) {
    rhs = new_binary_node(ND_MUL, rhs, new_num_node(lhs->type->base->size));
    return new_binary_node(kind, lhs, rhs);
  } else {
    lhs = new_binary_node(ND_MUL, lhs, new_num_node(rhs->type->base->size));
    return new_binary_node(kind, lhs, rhs);
  }
}

Node* postfix() {
  Node* node = primary();
  if (!consume("[")) {
    return node;
  }
  Node* index = expr();
  expect("]");
  return new_unary_node(ND_DEREF, new_add_node(ND_ADD, node, index));
}

Node* unary() {
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

Node* mul() {
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

Node* add() {
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

Node* relational() {
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

Node* equality() {
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

Node* assign() {
  Node* node = equality();

  for (;;) {
    if (consume("=")) {
      node = new_binary_node(ND_ASSIGN, node, equality());
    } else {
      return node;
    }
  }
}

Node* expr() { return assign(); }

Type* type_head() {
  Type* cur;
  if (consume("char")) {
    cur = add_size(new_type(TY_CHAR));
  } else {
    expect("int");
    cur = add_size(new_type(TY_INT));
  }

  while (consume("*")) {
    Type* head = add_size(new_type(TY_PTR));
    head->base = cur;
    cur = head;
  }
  return cur;
}

Type* type_tail(Type* head) {
  expect("[");
  int len = expect_num();
  expect("]");

  Type* array = new_type(TY_ARRAY);
  array->len = len;
  array->base = head;
  return add_size(array);
}

Node* lvar() {
  Type* ty = type_head();

  if (token->kind != TK_IDENT) {
    error_at(token->str, "expected an ident");
  }
  Token* ident = token;
  token = token->next;

  if (equal(token, "[")) {
    ty = type_tail(ty);
  }

  Obj* var = find_or_new_lvar(ty, ident->str, ident->len);
  return new_var_node(var);
}

bool equal_typename(Token* tok) {
  static char* tnames[] = {"int", "char"};
  int len = sizeof(tnames) / sizeof(char*);
  for (int i = 0; i < len; i++) {
    if (equal(tok, tnames[i])) {
      return true;
    }
  }
  return false;
}

Node* bloc_stmt();

Node* stmt() {
  if (equal(token, "{")) {
    return bloc_stmt();
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
  } else if (equal_typename(token)) {
    Node* node = lvar();
    expect(";");
    return node;
  } else {
    Node* node = expr();
    expect(";");
    return node;
  }
}

Node* bloc_stmt() {
  expect("{");
  Node head = {};
  Node* curr = &head;
  while (!consume("}")) {
    curr->next = stmt();
    curr = curr->next;
  }

  Node* node = new_node(ND_BLOCK);
  node->body = head.next;
  return node;
}

void gvar() {
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
}

void func() {
  Type* ty = type_head();

  if (token->kind != TK_IDENT) {
    error_at(token->str, "expected an ident");
  }
  Obj* func = new_obj(OJ_FUNC);
  func->type = ty;
  func->name = token->str;
  func->len = token->len;
  token = token->next;

  expect("(");
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

  func->body = bloc_stmt();

  func->lvars = lvars;
  lvars = NULL;

  add_code(func);
}

void program() {
  codes = NULL;
  gvars = NULL;
  lvars = NULL;

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
}