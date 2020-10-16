#include "cc.h"

int label_count = 0;
char* arg_regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

void genln(char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  printf("\n");
}

void gen(Node* node);

void gen_lval(Node* node) {
  switch (node->kind) {
    case ND_VAR:
      genln("  mov rax, rbp");
      genln("  sub rax, %d", node->offset);
      genln("  push rax");
      break;
    case ND_DEREF:
      gen(node->lhs);
      break;
    default:
      error("non left value");
  }
}

int sum_local_var_size() {
  int sum = 0;
  for (Var* var = local_vars; var; var = var->next) {
    sum += var->type->size;
  }
  return sum;
}

void gen(Node* node) {
  switch (node->kind) {
    case ND_FUNC: {
      genln(".global %.*s", node->len, node->name);
      genln("%.*s:", node->len, node->name);
      genln("  push rbp");
      genln("  mov rbp, rsp");
      genln("  sub rsp, %d", sum_local_var_size());
      int i = 0;
      for (Node* param = node->params; param; param = param->next) {
        gen_lval(param);
        genln("  pop rax");
        genln("  mov [rax], %s", arg_regs[i++]);
      }

      gen(node->body);
      genln("  pop rax");

      genln("  mov rsp, rbp");
      genln("  pop rbp");
      genln("  ret");
      return;
    }
    case ND_BLOCK:
      for (Node* body = node->body; body; body = body->next) {
        gen(body);
      }
      return;
    case ND_IF: {
      gen(node->cond);
      int lelse = label_count++;
      int lend = label_count++;
      genln("  pop rax");
      genln("  cmp rax, 0");
      genln("  je .Lelse%d", lelse);
      gen(node->then);
      genln(".Lelse%d:", lelse);
      if (node->els) {
        gen(node->els);
      }
      genln(".Lend%d:", lend);
      return;
    }
    case ND_FOR: {
      if (node->init) {
        gen(node->init);
      }
      int lbegin = label_count++;
      int lend = label_count++;
      genln(".Lbegin%d:", lbegin);
      if (node->cond) {
        gen(node->cond);
        genln("  pop rax");
        genln("  cmp rax, 0");
        genln("  je .Lend%d", lend);
      }
      gen(node->then);
      if (node->inc) {
        gen(node->inc);
      }
      genln("  jmp .Lbegin%d", lbegin);
      genln(".Lend%d:", lend);
      return;
    }
    case ND_RETURN:
      gen(node->lhs);
      genln("  pop rax");
      genln("  mov rsp, rbp");
      genln("  pop rbp");
      genln("  ret");
      return;
    case ND_ASSIGN:
      gen_lval(node->lhs);
      gen(node->rhs);
      genln("  pop rdi");
      genln("  pop rax");
      genln("  mov [rax], rdi");
      genln("  push rdi");
      return;
    case ND_ADDR:
      gen_lval(node->lhs);
      return;
    case ND_DEREF:
      gen(node->lhs);
      genln("  pop rax");
      genln("  mov rax, [rax]");
      genln("  push rax");
      return;
    case ND_NUM:
      genln("  push %d", node->val);
      return;
    case ND_VAR:
      gen_lval(node);
      genln("  pop rax");
      if (node->type->kind != TY_ARRAY) {
        genln("  mov rax, [rax]");
      }
      genln("  push rax");
      return;
    case ND_FUNCCALL: {
      int arg_count = 0;
      for (Node* arg = node->args; arg; arg = arg->next) {
        gen(arg);
        arg_count++;
      }
      for (int i = arg_count - 1; i >= 0; i--) {
        genln("  pop %s", arg_regs[i]);
      }
      genln("  call %.*s", node->len, node->name);
      genln("  push rax");
      return;
    }
    default:
      break;
  }

  gen(node->lhs);
  gen(node->rhs);

  genln("  pop rdi");
  genln("  pop rax");
  switch (node->kind) {
    case ND_EQ:
      genln("  cmp rax, rdi");
      genln("  sete al");
      genln("  movzb rax, al");
      break;
    case ND_NE:
      genln("  cmp rax, rdi");
      genln("  setne al");
      genln("  movzb rax, al");
      break;
    case ND_LT:
      genln("  cmp rax, rdi");
      genln("  setl al");
      genln("  movzb rax, al");
      break;
    case ND_LE:
      genln("  cmp rax, rdi");
      genln("  setle al");
      genln("  movzb rax, al");
      break;
    case ND_ADD:
      genln("  add rax, rdi");
      break;
    case ND_SUB:
      genln("  sub rax, rdi");
      break;
    case ND_MUL:
      genln("  imul rax, rdi");
      break;
    case ND_DIV:
      genln("  cqo");
      genln("  idiv rdi");
      break;
    default:
      break;
  }
  genln("  push rax");
}

void gen_program() {
  genln(".intel_syntax noprefix");
  for (Node* code = codes; code; code = code->next) {
    gen(code);
  }
}