#include "cc.h"

Node* codes;

Var* local_vars;

Type* new_type(TypeKind kind) {
  Type* type = calloc(1, sizeof(Type));
  type->kind = kind;
  return type;
}

Type* add_size(Type* type) {
  switch (type->kind) {
    case TY_INT:
    case TY_PTR:
      type->size = 8;
      break;
    case TY_ARRAY:
      type->size = type->array_of->size * type->len;
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
      node->type->ptr_to = node->lhs->type;
      break;
    case ND_DEREF:
      if (node->lhs->type->kind == TY_PTR) {
        node->type = node->lhs->type->ptr_to;
      } else if (node->lhs->type->kind == TY_ARRAY) {
        node->type = node->lhs->type->array_of;
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

Var* find_var(char* name, int len) {
  for (Var* var = local_vars; var; var = var->next) {
    if (var->len == len && memcmp(var->name, name, len) == 0) {
      return var;
    }
  }
  return NULL;
}

Var* new_var(Type* type, char* name, int len) {
  Var* var = calloc(1, sizeof(Var));
  var->next = local_vars;
  var->type = type;
  var->name = name;
  var->len = len;
  var->offset = (local_vars) ? local_vars->offset + type->size : type->size;
  local_vars = var;
  return var;
}

Var* find_or_new_var(Type* type, char* name, int len) {
  Var* var = find_var(name, len);
  if (var) {
    return var;
  }
  return new_var(type, name, len);
}

Node* new_var_node(Type* ty, int offset) {
  Node* node = new_node(ND_VAR);
  node->type = ty;
  node->offset = offset;
  return node;
}

Type* type() {
  expect("int");
  Type* cur = add_size(new_type(TY_INT));

  while (consume("*")) {
    Type* head = add_size(new_type(TY_PTR));
    head->ptr_to = cur;
    cur = head;
  }
  return cur;
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

    Var* var = find_var(token->str, token->len);
    if (!var) {
      error_at(token->str, "undefined ident");
    }
    Node* node = new_var_node(var->type, var->offset);
    token = token->next;
    return node;
  }

  return new_num_node(expect_num());
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
  if (lhs->type->kind == TY_INT && rhs->type->kind == TY_INT) {
    return new_binary_node(kind, lhs, rhs);
  } else if (is_pointable(lhs) && rhs->type->kind == TY_INT) {
    rhs = new_binary_node(ND_MUL, rhs, new_num_node(rhs->type->size));
    return new_binary_node(kind, lhs, rhs);
  } else {
    lhs = new_binary_node(ND_MUL, lhs, new_num_node(rhs->type->size));
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
  } else if (equal(token, "int")) {
    Type* ty = type();

    if (token->kind != TK_IDENT) {
      error_at(token->str, "expected an ident");
    }
    Token* ident = token;
    token = token->next;

    if (consume("[")) {
      int len = expect_num();
      expect("]");
      Type* array = new_type(TY_ARRAY);
      array->len = len;
      array->array_of = ty;
      ty = add_size(array);
    }

    Var* var = find_or_new_var(ty, ident->str, ident->len);
    Node* node = new_var_node(var->type, var->offset);

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

Node* func_def() {
  Type* ty = type();

  if (token->kind != TK_IDENT) {
    error_at(token->str, "expected an ident");
  }
  Node* node = new_node(ND_FUNC);
  node->type = ty;
  node->name = token->str;
  node->len = token->len;
  token = token->next;

  expect("(");
  Node head = {};
  Node* cur = &head;
  while (!consume(")")) {
    if (cur != &head) {
      expect(",");
    }

    Type* ty = type();

    if (token->kind != TK_IDENT) {
      error_at(token->str, "expected an ident");
    }
    Var* var = find_or_new_var(ty, token->str, token->len);
    cur->next = new_var_node(var->type, var->offset);
    cur = cur->next;
    token = token->next;
  }
  node->params = head.next;

  node->body = bloc_stmt();
  return node;
}

void program() {
  Node head = {};
  Node* cur = &head;
  while (!at_eof()) {
    cur->next = func_def();
    cur = cur->next;
  }
  codes = head.next;
}