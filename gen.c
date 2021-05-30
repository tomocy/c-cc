#include <stdio.h>
#include "cc.h"

static FILE* output_file;

static int depth_outside_frame = 0;
static int depth = 0;

static int func_count = 0;

static char* arg_regs8[] = {"dil", "sil", "dl", "cl", "r8b", "r9b"};
static char* arg_regs16[] = {"di", "si", "dx", "cx", "r8w", "r9w"};
static char* arg_regs32[] = {"edi", "esi", "edx", "ecx", "r8d", "r9d"};
static char* arg_regs64[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

static char s8to32[] = "movsx eax, al";
static char s16to32[] = "movsx eax, ax";
static char s32to64[] = "movsxd rax, eax";
static char z8to32[] = "movzx eax, al";
static char z16to32[] = "movzx eax, ax";
static char z32to64[] = "mov eax, eax";
static char* cast_table[][8] = {
  // to i8, i16, i32, i64, u8, u16, u32, u64
  {NULL, NULL, NULL, s32to64, z8to32, z16to32, NULL, s32to64},       // from i8
  {s8to32, NULL, NULL, s32to64, z8to32, z16to32, NULL, s32to64},     // from i16
  {s8to32, s16to32, NULL, s32to64, z8to32, z16to32, NULL, s32to64},  // from i32
  {s8to32, s16to32, NULL, NULL, z8to32, z16to32, NULL, NULL},        // from i64
  {s8to32, NULL, NULL, s32to64, NULL, NULL, NULL, s32to64},          // from u8
  {s8to32, s16to32, NULL, s32to64, z8to32, NULL, NULL, s32to64},     // from u16
  {s8to32, s16to32, NULL, z32to64, z8to32, z16to32, NULL, z32to64},  // from u32
  {s8to32, s16to32, NULL, NULL, z8to32, z16to32, NULL, NULL},        // from u64
};

enum { I8, I16, I32, I64, U8, U16, U32, U64 };

static int get_type_id(Type* type) {
  switch (type->kind) {
    case TY_CHAR:
      return type->is_unsigned ? U8 : I8;
    case TY_SHORT:
      return type->is_unsigned ? U16 : I16;
    case TY_INT:
      return type->is_unsigned ? U32 : I32;
    default:
      return type->is_unsigned ? U64 : I64;
  }
}

static void genln(char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(output_file, fmt, args);
  fprintf(output_file, "\n");
}

static void do_push(int* count, char* reg) {
  genln("  push %s", reg);
  (*count)++;
}

static void do_pop(int* count, char* reg) {
  genln("  pop %s", reg);
  (*count)--;
}

static void push_outside_frame(char* reg) {
  do_push(&depth_outside_frame, reg);
}

static void pop_outside_frame(char* reg) {
  do_pop(&depth_outside_frame, reg);
}

static void push(char* reg) {
  do_push(&depth, reg);
}

static void pop(char* reg) {
  do_pop(&depth, reg);
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
  if (node->type->kind == TY_ARRAY || node->type->kind == TY_STRUCT || node->type->kind == TY_UNION) {
    return;
  }

  char* ins = node->type->is_unsigned ? "movz" : "movs";

  if (node->type->size == 1) {
    genln("  %sx eax, BYTE PTR [rax]", ins);
    return;
  }

  if (node->type->size == 2) {
    genln("  %sx eax, WORD PTR [rax]", ins);
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

  int from_id = get_type_id(from);
  int to_id = get_type_id(to);
  char* ins = cast_table[from_id][to_id];
  if (ins) {
    genln("  %s", ins);
  }
}

// NOLINTNEXTLINE
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
      if (depth % 2 == 0) {
        genln("  call %s", node->name);
      } else {
        genln("  sub rsp, 8");
        genln("  call %s", node->name);
        genln("  add rsp, 8");
      }

      char* ins = node->type->is_unsigned ? "movz" : "movs";
      switch (node->type->kind) {
        case TY_BOOL:
          genln("  movzx eax, al");
          break;
        case TY_CHAR:
          genln("  %sx eax, al", ins);
          break;
        case TY_SHORT:
          genln("  %sx eax, ax", ins);
          break;
        default: {
        }
      }

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

  char* ax;
  char* di;
  char* dx;
  if (node->lhs->type->size <= 4 && node->rhs->type->size <= 4) {
    ax = "eax";
    di = "edi";
    dx = "edx";
  } else {
    ax = "rax";
    di = "rdi";
    dx = "rdx";
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
      if (node->lhs->type->is_unsigned) {
        genln("  setb al");
      } else {
        genln("  setl al");
      }
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
      if (node->lhs->type->is_unsigned) {
        genln("  shr %s, cl", ax);
      } else {
        genln("  sar %s, cl", ax);
      }
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
      if (node->type->is_unsigned) {
        genln("  mov %s, 0", dx);
        genln("  div %s", di);
      } else {
        if (node->rhs->type->size == 8) {
          genln("  cqo");
        } else {
          genln("  cdq");
        }
        genln("  idiv %s", di);
      }
      return;
    case ND_MOD:
      if (node->type->is_unsigned) {
        genln("  mov %s, 0", dx);
        genln("  div %s", di);
      } else {
        if (node->rhs->type->size == 8) {
          genln("  cqo");
        } else {
          genln("  cdq");
        }
        genln("  idiv %s", di);
      }

      genln("  mov rax, rdx");
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
    push_outside_frame("rbp");
    genln("  mov rbp, rsp");
    genln("  sub rsp, %d", func->stack_size);

    if (func->va_area) {
      int i = 0;
      for (Node* param = func->params; param; param = param->next) {
        i++;
      }

      int offset = func->va_area->offset;

      // to assign __va_area__ to __va_elem
      // set __va_area__ as __va_elem manually in memory
      // __va_area_.gp_offset (int)
      genln("  mov DWORD PTR [rbp - %d], %d", offset, i * 8);
      // __va_area_.fp_offset (int)
      genln("  mov DWORD PTR [rbp - %d], %d", offset - 4, 0);
      // __va_area_.reg_save_area (void*)
      genln("  mov QWORD PTR [rbp - %d], rbp", offset - 16);
      genln("  sub QWORD PTR [rbp - %d], %d", offset - 16, offset - 24);
      genln("  mov QWORD PTR [rbp - %d], rdi", offset - 24);
      genln("  mov QWORD PTR [rbp - %d], rsi", offset - 32);
      genln("  mov QWORD PTR [rbp - %d], rdx", offset - 40);
      genln("  mov QWORD PTR [rbp - %d], rcx", offset - 48);
      genln("  mov QWORD PTR [rbp - %d], r8", offset - 56);
      genln("  mov QWORD PTR [rbp - %d], r9", offset - 64);
      genln("  movsd [rbp - %d], xmm0", offset - 72);
      genln("  movsd [rbp - %d], xmm1", offset - 80);
      genln("  movsd [rbp - %d], xmm2", offset - 88);
      genln("  movsd [rbp - %d], xmm3", offset - 96);
      genln("  movsd [rbp - %d], xmm4", offset - 104);
      genln("  movsd [rbp - %d], xmm5", offset - 112);
      genln("  movsd [rbp - %d], xmm6", offset - 120);
      genln("  movsd [rbp - %d], xmm7", offset - 128);
    }

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
    pop_outside_frame("rbp");
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

static void assert_depth_offset(char* name, int d) {
  if (d != 0) {
    error("%s: push and pop do not offset each other: %d", name, d);
  }
}

void gen(Obj* codes) {
  open_output_file();

  genln(".intel_syntax noprefix");
  genln(".file 1 \"%s\"", input_filename);
  gen_data(codes);
  gen_text(codes);

  assert_depth_offset("depth_outside_frame", depth_outside_frame);
  assert_depth_offset("depth", depth);
}