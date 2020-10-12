#include "cc.h"

Node* stmts[100];

Ident* idents;

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

Ident* find_ident(Token* tok) {
  if (tok->kind != TK_IDENT) {
    error("non ident");
  }

  for (Ident* ident = idents; ident; ident = ident->next) {
    if (ident->len == tok->len &&
        memcmp(ident->name, tok->str, tok->len) == 0) {
      return ident;
    }
  }

  return NULL;
}

Node* new_node_ident(Token* tok) {
  Node* node = calloc(1, sizeof(Node));
  node->kind = ND_IDENT;

  Ident* ident = find_ident(tok);
  if (ident) {
    node->offset = ident->offset;
  } else {
    ident = calloc(1, sizeof(Ident));
    ident->next = idents;
    ident->name = tok->str;
    ident->len = tok->len;
    ident->offset = (idents) ? idents->offset + 8 : 8;
    node->offset = ident->offset;
    idents = ident;
  }
  return node;
}

Node* expr();

Node* primary() {
  if (consume("(")) {
    Node* node = expr();
    expect(")");
    return node;
  }

  if (token->kind == TK_IDENT) {
    Node* node = new_node_ident(token);
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

Node* stmt() {
  Node* node = expr();
  expect(";");
  return node;
}

void program() {
  int i = 0;
  while (!at_eof()) {
    stmts[i++] = stmt();
  }
  stmts[i] = NULL;
}