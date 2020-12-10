#include "cc.h"

static FILE* output_file;
static int depth = 0;
static int func_count = 0;
static char* arg_regs8[] = {"dil", "sil", "dl", "cl", "r8b", "r9b"};
static char* arg_regs16[] = {"di", "si", "dx", "cx", "r8w", "r9w"};
static char* arg_regs32[] = {"edi", "esi", "edx", "ecx", "r8d", "r9d"};
static char* arg_regs64[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

static void genln(char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(output_file, fmt, args);
  fprintf(output_file, "\n");
}

static void push_reg(char* reg) {
  genln("  push %s", reg);
  depth++;
}

static void pop(char* reg) {
  genln("  pop %s", reg);
  depth--;
}

static int count_label() {
  static int count = 0;
  return ++count;
}

static void gen_expr(Node* node);
static void gen_stmt(Node* node);

static void gen_addr(Node* node) {
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
    case ND_MEMBER:
      gen_addr(node->lhs);
      genln("  add rax, %d", node->offset);
      break;
    default:
      error("expected a left value: %d", node->kind);
  }
}

static void load(Node* node, char* dst, char* src) {
  if (node->type->kind == TY_ARRAY || node->type->kind == TY_STRUCT ||
      node->type->kind == TY_UNION) {
    return;
  }
  if (node->type->size == 1) {
    genln("  movsx %s, BYTE PTR [%s]", dst, src);
    return;
  }
  if (node->type->size == 2) {
    genln("  movsx %s, WORD PTR [%s]", dst, src);
    return;
  }
  if (node->type->size == 4) {
    genln("  movsx %s, DWORD PTR [%s]", dst, src);
    return;
  }
  genln("  mov %s, [%s]", dst, src);
}

static void cast(Type* to, Type* from) {
  if (to->kind == TY_VOID) {
    return;
  }

  if ((from->size == 1 && to->size == 2) ||
      (from->size == 2 && to->size == 1)) {
    genln("  movsbw ax, al");
    return;
  }

  if ((from->size == 1 && to->size == 4) ||
      (from->size == 4 && to->size == 1)) {
    genln("  movsbl eax, al");
    return;
  }

  if ((from->size == 1 && to->size == 8) ||
      (from->size == 8 && to->size == 1)) {
    genln("  movsbq rax, al");
    return;
  }

  if ((from->size == 2 && to->size == 4) ||
      (from->size == 4 && to->size == 2)) {
    genln("  movswl eax, ax");
    return;
  }

  if ((from->size == 2 && to->size == 8) ||
      (from->size == 8 && to->size == 2)) {
    genln("  movswq rax, ax");
    return;
  }

  if ((from->size == 4 && to->size == 8) ||
      (from->size == 8 && to->size == 4)) {
    genln("  movsxd rax, eax");
    return;
  }
}

static void gen_expr(Node* node) {
  genln("  .loc 1 %d", node->token->line);

  switch (node->kind) {
    case ND_ASSIGN:
      gen_addr(node->lhs);
      push_reg("rax");
      gen_expr(node->rhs);
      pop("rdi");
      if (node->type->kind == TY_STRUCT || node->type->kind == TY_UNION) {
        for (int i = 0; i < node->type->size; i++) {
          genln("  mov r8b, %d[rax]", i);
          genln("  mov %d[rdi], r8b", i);
        }
        return;
      }
      if (node->type->size == 1) {
        genln("  mov [rdi], al");
        return;
      }
      if (node->type->size == 2) {
        genln("  mov [rdi], ax");
        return;
      }
      if (node->type->size == 4) {
        genln("  mov [rdi], eax");
        return;
      }
      genln("  mov [rdi], rax");
      return;
    case ND_COMMA:
      gen_expr(node->lhs);
      gen_expr(node->rhs);
      return;
    case ND_CAST:
      gen_expr(node->lhs);
      cast(node->type, node->lhs->type);
      return;
    case ND_ADDR:
      gen_addr(node->lhs);
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
    case ND_MEMBER:
      gen_addr(node);
      load(node, "rax", "rax");
      return;
    case ND_NUM:
      genln("  mov rax, %ld", node->val);
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

  char *ax, *di;
  if (node->lhs->type->size <= 4 && node->rhs->type->size <= 4) {
    ax = "eax";
    di = "edi ";
  } else {
    ax = "rax";
    di = "rdi ";
  }

  switch (node->kind) {
    case ND_EQ:
      genln("  cmp %s, %s", ax, di);
      genln("  sete al");
      genln("  movzb rax, al");
      return;
    case ND_NE:
      genln("  cmp %s, %s", ax, di);
      genln("  setne al");
      genln("  movzb rax, al");
      return;
    case ND_LT:
      genln("  cmp %s, %s", ax, di);
      genln("  setl al");
      genln("  movzb rax, al");
      return;
    case ND_LE:
      genln("  cmp %s, %s", ax, di);
      genln("  setle al");
      genln("  movzb rax, al");
      return;
    case ND_ADD:
      genln("  add %s, %s", ax, di);
      return;
    case ND_SUB:
      genln("  sub %s, %s", ax, di);
      return;
    case ND_MUL:
      genln("  imul %s, %s", ax, di);
      return;
    case ND_DIV:
      if (node->rhs->type->size == 8) {
        genln("  cqo");
      } else {
        genln("  cdq");
      }
      genln("  idiv %s", di);
      return;
    default:
      error("expected an expression: %d", node->kind);
  }
}

static void gen_stmt(Node* node) {
  genln("  .loc 1 %d", node->token->line);

  switch (node->kind) {
    case ND_BLOCK:
      for (Node* body = node->body; body; body = body->next) {
        gen_stmt(body);
      }
      return;
    case ND_IF: {
      gen_expr(node->cond);
      int lelse = count_label();
      int lend = count_label();
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
      int lbegin = count_label();
      int lend = count_label();
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

static void gen_data(Obj* codes) {
  for (Obj* var = codes; var; var = var->next) {
    if (var->kind != OJ_GVAR) {
      continue;
    }

    genln(".data");
    genln(".global %s", var->name);
    genln("%s:", var->name);

    if (var->str_val) {
      for (int i = 0; i < var->type->len; i++) {
        genln("  .byte %d", var->str_val[i]);
      }
      continue;
    }

    if (var->int_val != 0) {
      genln("  .long %d", var->int_val);
      continue;
    }

    genln("  .zero %d", var->type->size);
  }
}

static void gen_text(Obj* codes) {
  for (Obj* func = codes; func; func = func->next) {
    if (func->kind != OJ_FUNC || !func->is_definition) {
      continue;
    }

    genln(".text");
    genln(".global %s", func->name);
    genln("%s:", func->name);
    push_reg("rbp");
    genln("  mov rbp, rsp");
    genln("  sub rsp, %d", func->stack_size);

    int i = 0;
    for (Node* param = func->params; param; param = param->next) {
      gen_addr(param);

      if (param->type->size == 1) {
        genln("  mov [rax], %s", arg_regs8[i++]);
        continue;
      }
      if (param->type->size == 2) {
        genln("  mov [rax], %s", arg_regs16[i++]);
        continue;
      }
      if (param->type->size == 4) {
        genln("  mov [rax], %s", arg_regs32[i++]);
        continue;
      }
      genln("  mov [rax], %s", arg_regs64[i++]);
    }

    gen_stmt(func->body);

    genln(".Lreturn%d:", func_count++);
    genln("  mov rsp, rbp");
    pop("rbp");
    genln("  ret");
  }
}

static void open_output_file() {
  if (!output_filename || strcmp(output_filename, "-") == 0) {
    output_file = stdout;
    return;
  }

  output_file = fopen(output_filename, "w");
  if (!output_file) {
    error("cannot open output file: %s: %s", output_filename, strerror(errno));
  }
}

void gen(Obj* codes) {
  open_output_file();

  genln(".intel_syntax noprefix");
  genln(".file 1 \"%s\"", input_filename);
  gen_data(codes);
  gen_text(codes);
  if (depth != 0) {
    error("push and pop do not offset each other: depth %d", depth);
  }
}