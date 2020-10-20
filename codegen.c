#include "cc.h"

int depth = 0;
int label_count = 0;
int func_count = 0;
char* arg_regs8[] = {"dil", "sil", "dl", "cl", "r8b", "r9b"};
char* arg_regs64[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

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

void gen_expr(Node* node);
void gen_stmt(Node* node);

void gen_lval(Node* node) {
  switch (node->kind) {
    case ND_GVAR:
      genln("  lea rax, %s[rip]", node->name);
      break;
    case ND_LVAR:
      genln("  mov rax, rbp");
      genln("  sub rax, %d", node->offset);
      break;
    case ND_DEREF:
      gen_expr(node->lhs);
      break;
    default:
      error("non left value");
  }
}

void load(Node* node, char* dst, char* src) {
  if (node->type->kind == TY_ARRAY) {
  } else if (node->type->kind == TY_CHAR) {
    genln("  movsx %s, BYTE PTR [%s]", dst, src);
  } else {
    genln("  mov %s, [%s]", dst, src);
  }
}

void gen_expr(Node* node) {
  switch (node->kind) {
    case ND_ASSIGN:
      gen_lval(node->lhs);
      push_reg("rax");
      gen_expr(node->rhs);
      pop("rdi");
      if (node->type->kind == TY_CHAR) {
        genln("  mov [rdi], al");
      } else {
        genln("  mov [rdi], rax");
      }
      return;
    case ND_ADDR:
      gen_lval(node->lhs);
      return;
    case ND_DEREF:
      gen_expr(node->lhs);
      load(node, "rax", "rax");
      return;
    case ND_FUNCCALL: {
      int arg_count = 0;
      for (Node* arg = node->args; arg; arg = arg->next) {
        gen_expr(arg);
        push_reg("rax");
        arg_count++;
      }
      for (int i = arg_count - 1; i >= 0; i--) {
        pop(arg_regs64[i]);
      }

      genln("  mov rax, 0");
      genln("  call %s", node->name);
      return;
    }
    case ND_GVAR:
    case ND_LVAR:
      gen_lval(node);
      load(node, "rax", "rax");
      return;
    case ND_NUM:
      push_val(node->val);
      pop("rax");
      return;
    case ND_STMT_EXPR:
      gen_stmt(node->body);
      return;
    case ND_EQ:
    case ND_NE:
    case ND_LT:
    case ND_LE:
    case ND_ADD:
    case ND_SUB:
    case ND_MUL:
    case ND_DIV:
      break;
    default:
      error("expected an expression: %d", node->kind);
  }

  gen_expr(node->lhs);
  push_reg("rax");
  gen_expr(node->rhs);
  genln("  mov rdi, rax");
  pop("rax");

  switch (node->kind) {
    case ND_EQ:
      genln("  cmp rax, rdi");
      genln("  sete al");
      genln("  movzb rax, al");
      return;
    case ND_NE:
      genln("  cmp rax, rdi");
      genln("  setne al");
      genln("  movzb rax, al");
      return;
    case ND_LT:
      genln("  cmp rax, rdi");
      genln("  setl al");
      genln("  movzb rax, al");
      return;
    case ND_LE:
      genln("  cmp rax, rdi");
      genln("  setle al");
      genln("  movzb rax, al");
      return;
    case ND_ADD:
      genln("  add rax, rdi");
      return;
    case ND_SUB:
      genln("  sub rax, rdi");
      return;
    case ND_MUL:
      genln("  imul rax, rdi");
      return;
    case ND_DIV:
      genln("  cqo");
      genln("  idiv rdi");
      return;
    default:
      error("expected an expression: %d", node->kind);
  }
}

void gen_stmt(Node* node) {
  switch (node->kind) {
    case ND_BLOCK:
      for (Node* body = node->body; body; body = body->next) {
        gen_stmt(body);
      }
      return;
    case ND_IF: {
      gen_expr(node->cond);
      int lelse = label_count++;
      int lend = label_count++;
      genln("  cmp rax, 0");
      genln("  je .Lelse%d", lelse);
      gen_stmt(node->then);
      genln(".Lelse%d:", lelse);
      if (node->els) {
        gen_stmt(node->els);
      }
      genln(".Lend%d:", lend);
      return;
    }
    case ND_FOR: {
      if (node->init) {
        gen_expr(node->init);
      }
      int lbegin = label_count++;
      int lend = label_count++;
      genln(".Lbegin%d:", lbegin);
      if (node->cond) {
        push_reg("rax");
        gen_expr(node->cond);
        genln("  cmp rax, 0");
        pop("rax");
        genln("  je .Lend%d", lend);
      }
      gen_stmt(node->then);
      if (node->inc) {
        push_reg("rax");
        gen_expr(node->inc);
        pop("rax");
      }
      genln("  jmp .Lbegin%d", lbegin);
      genln(".Lend%d:", lend);
      return;
    }
    case ND_RETURN:
      gen_expr(node->lhs);
      genln("  jmp .Lreturn%d", func_count);
      return;
    default:
      gen_expr(node);
  }
}

void gen_data() {
  for (Obj* var = codes; var; var = var->next) {
    if (var->kind != OJ_GVAR) {
      continue;
    }

    printf(".data\n");
    printf(".global %s\n", var->name);
    printf("%s:\n", var->name);
    if (var->data) {
      for (int i = 0; i < var->type->len; i++) {
        printf("  .byte %d\n", var->data[i]);
      }
    } else {
      printf("  .zero %d\n", var->type->size);
    }
  }
}

int sum_vars_size(Obj* vars) {
  int sum = 0;
  for (Obj* var = vars; var; var = var->next) {
    sum += var->type->size;
  }
  return sum;
}

int align(int n, int align) { return (n + align - 1) / align * align; }

void gen_text() {
  for (Obj* func = codes; func; func = func->next) {
    if (func->kind != OJ_FUNC) {
      continue;
    }

    genln(".text");
    genln(".global %s", func->name);
    genln("%s:", func->name);
    push_reg("rbp");
    genln("  mov rbp, rsp");
    genln("  sub rsp, %d", align(sum_vars_size(func->lvars), 16));
    int i = 0;
    for (Node* param = func->params; param; param = param->next) {
      gen_lval(param);
      if (param->type->kind == TY_CHAR) {
        genln("  mov [rax], %s", arg_regs8[i++]);
      } else {
        genln("  mov [rax], %s", arg_regs64[i++]);
      }
    }

    gen_stmt(func->body);

    genln(".Lreturn%d:", func_count++);
    genln("  mov rsp, rbp");
    pop("rbp");
    genln("  ret");
  }
}

void gen_program() {
  genln(".intel_syntax noprefix");
  gen_data();
  gen_text();
  if (depth != 0) {
    error("push and pop do not offset each other: depth %d", depth);
  }
}