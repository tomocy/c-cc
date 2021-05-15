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

static void push(char* reg) {
  genln("  push %s", reg);
  depth++;
}

static void pop(char* reg) {
  genln("  pop %s", reg);
  depth--;
}

static int count_label(void) {
  static int count = 0;
  return count++;
}

static void gen_expr(Node* node);
static void gen_stmt(Node* node);

static void gen_addr(Node* node) {
  switch (node->kind) {
    case ND_COMMA:
      gen_expr(node->lhs);
      gen_addr(node->rhs);
      break;
    case ND_DEREF:
      gen_expr(node->lhs);
      break;
    case ND_GVAR:
      genln("  lea rax, %s[rip]", node->name);
      break;
    case ND_LVAR:
      genln("  mov rax, rbp");
      genln("  sub rax, %d", node->offset);
      break;
    case ND_MEMBER:
      gen_addr(node->lhs);
      genln("  add rax, %d", node->offset);
      break;
    default:
      error_token(node->token, "expected a left value");
  }
}

static void load(Node* node) {
  if (node->type->kind == TY_ARRAY || node->type->kind == TY_STRUCT ||
      node->type->kind == TY_UNION) {
    return;
  }

  if (node->type->size == 1) {
    genln("  movsx eax, BYTE PTR [rax]");
    return;
  }

  if (node->type->size == 2) {
    genln("  movsx eax, WORD PTR [rax]");
    return;
  }

  if (node->type->size == 4) {
    genln("  movsx rax, DWORD PTR [rax]");
    return;
  }

  genln("  mov rax, [rax]");
}

static void cast(Type* to, Type* from) {
  if (to->kind == TY_VOID) {
    return;
  }

  if (to->kind == TY_BOOL) {
    if (is_numable(from) && from->size <= 4) {
      genln("  cmp eax, 0");
    } else {
      genln("  cmp rax, 0");
    }
    genln("  setne al");
    genln("  movzx rax, al");
    return;
  }

  if ((from->size == 1 && to->size == 2) ||
      (from->size == 2 && to->size == 1)) {
    genln("  movsx ax, al");
    return;
  }

  if ((from->size == 1 && to->size == 4) ||
      (from->size == 4 && to->size == 1)) {
    genln("  movsx eax, al");
    return;
  }

  if ((from->size == 1 && to->size == 8) ||
      (from->size == 8 && to->size == 1)) {
    genln("  movsx rax, al");
    return;
  }

  if ((from->size == 2 && to->size == 4) ||
      (from->size == 4 && to->size == 2)) {
    genln("  movsx eax, ax");
    return;
  }

  if ((from->size == 2 && to->size == 8) ||
      (from->size == 8 && to->size == 2)) {
    genln("  movsx rax, ax");
    return;
  }

  if ((from->size == 4 && to->size == 8) ||
      (from->size == 8 && to->size == 4)) {
    genln("  movsx rax, eax");
    return;
  }
}

static void gen_expr(Node* node) {
  genln("  .loc 1 %d", node->token->line);

  switch (node->kind) {
    case ND_ASSIGN:
      gen_addr(node->lhs);
      push("rax");
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
    case ND_COND: {
      int label = count_label();
      gen_expr(node->cond);
      genln("  cmp rax, 0");
      genln("  je .Lelse%d", label);

      gen_expr(node->then);
      genln("  jmp .Lend%d", label);

      genln(".Lelse%d:", label);
      gen_expr(node->els);

      genln(".Lend%d:", label);
      return;
    }
    case ND_CAST:
      gen_expr(node->lhs);
      cast(node->type, node->lhs->type);
      return;
    case ND_NEG:
      gen_expr(node->lhs);
      genln("  neg rax");
      return;
    case ND_ADDR:
      gen_addr(node->lhs);
      return;
    case ND_DEREF:
      gen_expr(node->lhs);
      load(node);
      return;
    case ND_NOT:
      gen_expr(node->lhs);
      genln("  cmp rax, 0");
      genln("  sete al");
      genln("  movzx rax, al");
      return;
    case ND_BITNOT:
      gen_expr(node->lhs);
      genln("  not rax");
      return;
    case ND_STMT_EXPR:
      for (Node* stmt = node->body; stmt; stmt = stmt->next) {
        gen_stmt(stmt);
      }
      return;
    case ND_GVAR:
    case ND_LVAR:
    case ND_MEMBER:
      gen_addr(node);
      load(node);
      return;
    case ND_FUNCCALL: {
      int arg_count = 0;
      for (Node* arg = node->args; arg; arg = arg->next) {
        gen_expr(arg);
        push("rax");
        arg_count++;
      }
      for (int i = arg_count - 1; i >= 0; i--) {
        pop(arg_regs64[i]);
      }

      genln("  mov rax, 0");
      genln("  call %s", node->name);
      return;
    }
    case ND_NUM:
      genln("  mov rax, %ld", node->val);
      return;
    case ND_NULL:
      return;
    case ND_MEMZERO:
      genln("  mov rcx, %d", node->type->size);
      genln("  mov rdi, rbp");
      genln("  sub rdi, %d", node->offset);
      genln("  mov al, 0");
      genln("  rep stosb");
      return;
    case ND_OR:
    case ND_AND:
    case ND_BITOR:
    case ND_BITXOR:
    case ND_BITAND:
    case ND_EQ:
    case ND_NE:
    case ND_LT:
    case ND_LE:
    case ND_LSHIFT:
    case ND_RSHIFT:
    case ND_ADD:
    case ND_SUB:
    case ND_MUL:
    case ND_DIV:
    case ND_MOD:
      break;
    default:
      error_token(node->token, "expected an expression");
  }

  gen_expr(node->lhs);
  push("rax");
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
    case ND_OR: {
      int label = count_label();
      gen_expr(node->lhs);
      genln("  cmp rax, 0");
      genln("  jne .Ltrue%d", label);
      gen_expr(node->rhs);
      genln("  cmp rax, 0");
      genln("  jne .Ltrue%d", label);
      genln("  mov rax, 0");
      genln("  jmp .Lend%d", label);
      genln(".Ltrue%d:", label);
      genln("  mov rax, 1");
      genln(".Lend%d:", label);
      return;
    }
    case ND_AND: {
      int label = count_label();
      gen_expr(node->lhs);
      genln("  cmp rax, 0");
      genln("  je .Lfalse%d", label);
      gen_expr(node->rhs);
      genln("  cmp rax, 0");
      genln("  je .Lfalse%d", label);
      genln("  mov rax, 1");
      genln("  jmp .Lend%d", label);
      genln(".Lfalse%d:", label);
      genln("  mov rax, 0");
      genln(".Lend%d:", label);
      return;
    }
    case ND_BITOR:
      genln("  or %s, %s", ax, di);
      return;
    case ND_BITXOR:
      genln("  xor %s, %s", ax, di);
      return;
    case ND_BITAND:
      genln("  and %s, %s", ax, di);
      return;
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
    case ND_LSHIFT:
      genln("  mov rcx, rdi");
      genln("  shl %s, cl", ax);
      return;
    case ND_RSHIFT:
      genln("  mov rcx, rdi");
      genln("  sar %s, cl", ax);
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
    case ND_MOD:
      if (node->rhs->type->size == 8) {
        genln("  cqo");
      } else {
        genln("  cdq");
      }
      genln("  idiv %s", di);

      if (node->kind == ND_MOD) {
        genln("  mov rax, rdx");
      }
      return;
    default:
      error_token(node->token, "expected an expression");
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
      genln("  jmp .Lend%d", lend);

      genln(".Lelse%d:", lelse);
      if (node->els) {
        gen_stmt(node->els);
      }

      genln(".Lend%d:", lend);
      return;
    }
    case ND_SWITCH:
      gen_expr(node->cond);

      char* r = node->cond->type->size == 8 ? "rax" : "eax";
      for (Node* c = node->cases; c; c = c->cases) {
        genln("  cmp %s, %ld", r, c->val);
        genln("  je %s", c->label_id);
      }
      if (node->default_label_id) {
        genln("  jmp %s", node->default_label_id);
      }
      genln("  jmp %s", node->break_label_id);

      gen_stmt(node->then);

      genln("%s:", node->break_label_id);
      return;
    case ND_CASE:
      genln("%s:", node->label_id);
      gen_stmt(node->lhs);
      return;
    case ND_FOR: {
      if (node->init) {
        gen_stmt(node->init);
      }
      int lbegin = count_label();
      genln(".Lbegin%d:", lbegin);
      if (node->cond) {
        push("rax");
        gen_expr(node->cond);
        genln("  cmp rax, 0");
        pop("rax");
        genln("  je %s", node->break_label_id);
      }
      gen_stmt(node->then);
      genln("%s:", node->continue_label_id);
      if (node->inc) {
        push("rax");
        gen_expr(node->inc);
        pop("rax");
      }
      genln("  jmp .Lbegin%d", lbegin);
      genln("%s:", node->break_label_id);
      return;
    }
    case ND_RETURN:
      if (node->lhs) {
        gen_expr(node->lhs);
      }
      genln("  jmp .Lreturn%d", func_count);
      return;
    case ND_LABEL:
      genln("%s:", node->label_id);
      gen_stmt(node->lhs);
      return;
    case ND_GOTO:
      genln("  jmp %s", node->label_id);
      return;
    case ND_EXPR_STMT:
      gen_expr(node->lhs);
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
    if (!var->is_definition) {
      continue;
    }

    if (var->is_static) {
      genln(".local %s", var->name);
    } else {
      genln(".global %s", var->name);
    }

    if (var->val) {
      genln(".data");
      genln(".align %d", var->alignment);
      genln("%s:", var->name);

      Relocation* reloc = var->relocs;
      int offset = 0;
      while (offset < var->type->size) {
        if (reloc && offset == reloc->offset) {
          genln("  .quad %s%+ld", reloc->label, reloc->addend);
          reloc = reloc->next;
          offset += 8;
          continue;
        }

        genln("  .byte %d", var->val[offset++]);
      }

      continue;
    }

    genln(".bss");
    genln(".align %d", var->alignment);
    genln("%s:", var->name);
    genln("  .zero %d", var->type->size);
  }
}

static void gen_text(Obj* codes) {
  for (Obj* func = codes; func; func = func->next) {
    if (func->kind != OJ_FUNC || !func->is_definition) {
      continue;
    }

    genln(".text");
    if (func->is_static) {
      genln(".local %s", func->name);
    } else {
      genln(".global %s", func->name);
    }
    genln("%s:", func->name);
    push("rbp");
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

static void open_output_file(void) {
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