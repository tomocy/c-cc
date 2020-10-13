#include "cc.h"

Node* codes;

Var* local_vars;

Node* new_node(NodeKind kind) {
  Node* node = calloc(1, sizeof(Node));
  node->kind = kind;
  return node;
}

Node* new_unary_node(NodeKind kind, Node* lhs) {
  Node* node = new_node(kind);
  node->lhs = lhs;
  return node;
}

Node* new_binary_node(NodeKind kind, Node* lhs, Node* rhs) {
  Node* node = new_node(kind);
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

Node* new_num_node(int val) {
  Node* node = new_node(ND_NUM);
  node->val = val;
  return node;
}

Var* find_var(Token* tok) {
  if (tok->kind != TK_IDENT) {
    error("non ident");
  }

  for (Var* var = local_vars; var; var = var->next) {
    if (var->len == tok->len && memcmp(var->name, tok->str, tok->len) == 0) {
      return var;
    }
  }

  return NULL;
}

Node* new_var_node(Token* tok) {
  Node* node = new_node(ND_VAR);

  Var* var = find_var(tok);
  if (var) {
    node->offset = var->offset;
  } else {
    var = calloc(1, sizeof(Var));
    var->next = local_vars;
    var->name = tok->str;
    var->len = tok->len;
    var->offset = (local_vars) ? local_vars->offset + 8 : 8;
    node->offset = var->offset;
    local_vars = var;
  }
  return node;
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

    Node* node = new_var_node(token);
    token = token->next;
    return node;
  }

  return new_num_node(expect_num());
}

Node* unary() {
  if (consume("+")) {
    return primary();
  } else if (consume("-")) {
    return new_binary_node(ND_SUB, new_num_node(0), primary());
  } else {
    return primary();
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
      node = new_binary_node(ND_ADD, node, mul());
    } else if (consume("-")) {
      node = new_binary_node(ND_SUB, node, mul());
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

Node* func_decl() {
  if (token->kind != TK_IDENT) {
    error_at(token->str, "expected ident");
  }

  Node* node = new_node(ND_FUNC);
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

    cur->next = new_var_node(token);
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
    cur->next = func_decl();
    cur = cur->next;
  }
  codes = head.next;
}