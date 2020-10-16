#include "cc.h"

int depth = 0;
int label_count = 0;
int func_count = 0;
char* arg_regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

void genln(char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  printf("\n");
}

void push_reg(char* reg) {
  genln("  push %s", reg);
  depth++;
}

void push_val(int v) {
  genln("  push %d", v);
  depth++;
}

void pop(char* reg) {
  genln("  pop %s", reg);
  depth--;
}

void gen(Node* node);

void gen_lval(Node* node) {
  switch (node->kind) {
    case ND_VAR:
      genln("  mov rax, rbp");
      genln("  sub rax, %d", node->offset);
      break;
    case ND_DEREF:
      gen(node->lhs);
      break;
    default:
      error("non left value");
  }
}

int sum_vars_size(Var* vars) {
  int sum = 0;
  for (Var* var = vars; var; var = var->next) {
    sum += var->type->size;
  }
  return sum;
}

int align(int n, int align) { return (n + align - 1) / align * align; }

void gen(Node* node) {
  switch (node->kind) {
    case ND_FUNC: {
      genln(".global %.*s", node->len, node->name);
      genln("%.*s:", node->len, node->name);
      push_reg("rbp");
      genln("  mov rbp, rsp");
      genln("  sub rsp, %d", align(sum_vars_size(node->local_vars), 16));
      int i = 0;
      for (Node* param = node->params; param; param = param->next) {
        gen_lval(param);
        genln("  mov [rax], %s", arg_regs[i++]);
      }

      gen(node->body);

      genln(".Lreturn%d:", func_count++);
      genln("  mov rsp, rbp");
      pop("rbp");
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
        push_reg("rax");
        gen(node->cond);
        genln("  cmp rax, 0");
        pop("rax");
        genln("  je .Lend%d", lend);
      }
      gen(node->then);
      if (node->inc) {
        push_reg("rax");
        gen(node->inc);
        pop("rax");
      }
      genln("  jmp .Lbegin%d", lbegin);
      genln(".Lend%d:", lend);
      return;
    }
    case ND_RETURN:
      gen(node->lhs);
      genln("  jmp .Lreturn%d", func_count);
      return;
    case ND_ASSIGN:
      gen_lval(node->lhs);
      push_reg("rax");
      gen(node->rhs);
      genln("  mov rdi, rax");
      pop("rax");
      genln("  mov [rax], rdi");
      genln("  mov rax, rdi");
      return;
    case ND_ADDR:
      gen_lval(node->lhs);
      return;
    case ND_DEREF:
      gen(node->lhs);
      genln("  mov rax, [rax]");
      return;
    case ND_NUM:
      push_val(node->val);
      pop("rax");
      return;
    case ND_VAR:
      gen_lval(node);
      if (node->type->kind != TY_ARRAY) {
        genln("  mov rax, [rax]");
      }
      return;
    case ND_FUNCCALL: {
      int arg_count = 0;
      for (Node* arg = node->args; arg; arg = arg->next) {
        gen(arg);
        push_reg("rax");
        arg_count++;
      }
      for (int i = arg_count - 1; i >= 0; i--) {
        pop(arg_regs[i]);
      }
      genln("  call %.*s", node->len, node->name);
      return;
    }
    default:
      break;
  }

  gen(node->lhs);
  push_reg("rax");
  gen(node->rhs);
  genln("  mov rdi, rax");
  pop("rax");

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
}

void gen_program() {
  genln(".intel_syntax noprefix");
  for (Node* code = codes; code; code = code->next) {
    gen(code);
  }
  if (depth != 0) {
    error("push and pop do not offset each other: depth %d", depth);
  }
}