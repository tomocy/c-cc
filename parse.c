#include "cc.h"

Node* codes[100];

Var* local_vars;

Node* new_node(NodeKind kind, Node* lhs, Node* rhs) {
  Node* node = calloc(1, sizeof(Node));
  node->kind = kind;
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

Node* new_node_num(int val) {
  Node* node = calloc(1, sizeof(Node));
  node->kind = ND_NUM;
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

Node* new_node_var(Token* tok) {
  Node* node = calloc(1, sizeof(Node));
  node->kind = ND_VAR;

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
      Node* node = new_node(ND_FUNCCALL, NULL, NULL);
      node->name = token->str;
      node->len = token->len;
      token = token->next;
      node->args = func_args();
      return node;
    }

    Node* node = new_node_var(token);
    token = token->next;
    return node;
  }

  return new_node_num(expect_number());
}

Node* unary() {
  if (consume("+")) {
    return primary();
  } else if (consume("-")) {
    return new_node(ND_SUB, new_node_num(0), primary());
  } else {
    return primary();
  }
}

Node* mul() {
  Node* node = unary();

  for (;;) {
    if (consume("*")) {
      node = new_node(ND_MUL, node, unary());
    } else if (consume("/")) {
      node = new_node(ND_DIV, node, unary());
    } else {
      return node;
    }
  }
}

Node* add() {
  Node* node = mul();

  for (;;) {
    if (consume("+")) {
      node = new_node(ND_ADD, node, mul());
    } else if (consume("-")) {
      node = new_node(ND_SUB, node, mul());
    } else {
      return node;
    }
  }
}

Node* relational() {
  Node* node = add();

  for (;;) {
    if (consume("<")) {
      node = new_node(ND_LT, node, add());
    } else if (consume("<=")) {
      node = new_node(ND_LE, node, add());
    } else if (consume(">")) {
      node = new_node(ND_LT, add(), node);
    } else if (consume(">=")) {
      node = new_node(ND_LE, add(), node);
    } else {
      return node;
    }
  }
}

Node* equality() {
  Node* node = relational();

  for (;;) {
    if (consume("==")) {
      node = new_node(ND_EQ, node, relational());
    } else if (consume("!=")) {
      node = new_node(ND_NE, node, relational());
    } else {
      return node;
    }
  }
}

Node* assign() {
  Node* node = equality();

  for (;;) {
    if (consume("=")) {
      node = new_node(ND_ASSIGN, node, equality());
    } else {
      return node;
    }
  }
}

Node* expr() { return assign(); }

Node* bloc_stmt();

Node* stmt() {
  Node* node;
  if (equal(token, "{")) {
    return bloc_stmt();
  } else if (consume("if")) {
    expect("(");
    node = new_node(ND_IF, NULL, NULL);
    node->cond = expr();
    expect(")");
    node->then = stmt();
    if (consume("else")) {
      node->els = stmt();
    }
    return node;
  } else if (consume("while")) {
    expect("(");
    node = new_node(ND_FOR, NULL, NULL);
    node->cond = expr();
    expect(")");
    node->then = stmt();
    return node;
  } else if (consume("for")) {
    expect("(");
    node = new_node(ND_FOR, NULL, NULL);
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
    node = new_node(ND_RETURN, expr(), NULL);
  } else {
    node = expr();
  }
  expect(";");
  return node;
}

Node* bloc_stmt() {
  expect("{");
  Node head = {};
  Node* curr = &head;

  while (!consume("}")) {
    curr->next = stmt();
    curr = curr->next;
  }

  Node* node = new_node(ND_BLOCK, NULL, NULL);
  node->body = head.next;
  return node;
}

Node* func_decl() {
  if (token->kind != TK_IDENT) {
    error_at(token->str, "expected ident");
  }

  Node* node = new_node(ND_FUNC, NULL, NULL);
  node->name = token->str;
  node->len = token->len;
  token = token->next;
  expect("(");
  expect(")");
  node->body = bloc_stmt();
  return node;
}

void program() {
  int i = 0;
  while (!at_eof()) {
    codes[i++] = func_decl();
  }
  codes[i] = NULL;
}