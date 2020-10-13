#include "cc.h"

int label_count = 0;

void gen_lval(Node* node) {
  if (node->kind != ND_VAR) {
    error("non left value");
  }

  printf("  mov rax, rbp\n");
  printf("  sub rax, %d\n", node->offset);
  printf("  push rax\n");
}

void gen(Node* node) {
  switch (node->kind) {
    case ND_NUM:
      printf("  push %d\n", node->val);
      return;
    case ND_VAR:
      gen_lval(node);
      printf("  pop rax\n");
      printf("  mov rax, [rax]\n");
      printf("  push rax\n");
      return;
    case ND_FUNCCALL:
      printf("  call %.*s\n", node->len, node->name);
      printf("  push rax\n");
      return;
    case ND_ASSIGN:
      gen_lval(node->lhs);
      gen(node->rhs);
      printf("  pop rdi\n");
      printf("  pop rax\n");
      printf("  mov [rax], rdi\n");
      printf("  push rdi\n");
      return;
    case ND_RETURN:
      gen(node->lhs);
      printf("  pop rax\n");
      printf("  mov rsp, rbp\n");
      printf("  pop rbp\n");
      printf("  ret\n");
      return;
    case ND_IF: {
      gen(node->cond);
      int lelse = label_count++;
      int lend = label_count++;
      printf("  pop rax\n");
      printf("  cmp rax, 0\n");
      printf("  je .Lelse%d\n", lelse);
      gen(node->then);
      printf(".Lelse%d:\n", lelse);
      if (node->els) {
        gen(node->els);
      }
      printf(".Lend%d:\n", lend);
      return;
    }
    case ND_FOR: {
      if (node->init) {
        gen(node->init);
      }
      int lbegin = label_count++;
      int lend = label_count++;
      printf(".Lbegin%d:\n", lbegin);
      if (node->cond) {
        gen(node->cond);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je .Lend%d\n", lend);
      }
      gen(node->then);
      if (node->inc) {
        gen(node->inc);
      }
      printf("  jmp .Lbegin%d\n", lbegin);
      printf(".Lend%d:\n", lend);
      return;
    }
    case ND_BLOCK:
      for (Node* body = node->body; body; body = body->next) {
        gen(body);
      }
      return;
    default:
      break;
  }

  gen(node->lhs);
  gen(node->rhs);

  printf("  pop rdi\n");
  printf("  pop rax\n");

  switch (node->kind) {
    case ND_EQ:
      printf("  cmp rax, rdi\n");
      printf("  sete al\n");
      printf("  movzb rax, al\n");
      break;
    case ND_NE:
      printf("  cmp rax, rdi\n");
      printf("  setne al\n");
      printf("  movzb rax, al\n");
      break;
    case ND_LT:
      printf("  cmp rax, rdi\n");
      printf("  setl al\n");
      printf("  movzb rax, al\n");
      break;
    case ND_LE:
      printf("  cmp rax, rdi\n");
      printf("  setle al\n");
      printf("  movzb rax, al\n");
      break;
    case ND_ADD:
      printf("  add rax, rdi\n");
      break;
    case ND_SUB:
      printf("  sub rax, rdi\n");
      break;
    case ND_MUL:
      printf("  imul rax, rdi\n");
      break;
    case ND_DIV:
      printf("  cqo\n");
      printf("  idiv rdi\n");
    default:
      break;
  }

  printf("  push rax\n");
}

int count_local_vars() {
  int len = 0;
  for (Var* var = local_vars; var; var = var->next) {
    len++;
  }
  return len;
}

void gen_program() {
  printf(".intel_syntax noprefix\n");
  printf(".global main\n");
  printf("main:\n");

  printf("  push rbp\n");
  printf("  mov rbp, rsp\n");
  printf("  sub rsp, %d\n", 8 * count_local_vars());

  for (int i = 0; stmts[i]; i++) {
    gen(stmts[i]);
    printf("  pop rax\n");
  }

  printf("  mov rsp, rbp\n");
  printf("  pop rbp\n");

  printf("  ret\n");
}