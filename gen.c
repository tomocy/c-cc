#include "cc.h"

static FILE* output_file;

static int depth_outside_frame = 0;
static int depth = 0;

static int func_count = 0;

#define MAX_FLOAT_REG_ARGS 8
#define MAX_INT_REG_ARGS 6

static char* arg_regs8[] = {"dil", "sil", "dl", "cl", "r8b", "r9b"};
static char* arg_regs16[] = {"di", "si", "dx", "cx", "r8w", "r9w"};
static char* arg_regs32[] = {"edi", "esi", "edx", "ecx", "r8d", "r9d"};
static char* arg_regs64[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

static char s8_32[] = "movsx eax, al";
static char s16_32[] = "movsx eax, ax";
static char s32_64[] = "movsxd rax, eax";
static char i32_f32[] = "cvtsi2ss xmm0, eax";
static char i32_f64[] = "cvtsi2sd xmm0, eax";
static char i64_f32[] = "cvtsi2ss xmm0, rax";
static char i64_f64[] = "cvtsi2sd xmm0, rax";
static char z8_32[] = "movzx eax, al";
static char z16_32[] = "movzx eax, ax";
static char z32_64[] = "mov eax, eax";
static char u32_f32[] = "mov eax, eax; cvtsi2ss xmm0, rax";
static char u32_f64[] = "mov eax, eax; cvtsi2sd xmm0, rax";
static char u64_f32[] = "cvtsi2ss xmm0, rax";
static char u64_f64[]
  = "test rax, rax; js 1f; pxor xmm0, xmm0; cvtsi2sd xmm0, rax; jmp 2f; "
    "1: mov rdi, rax; and eax, 1; pxor xmm0, xmm0; shr rdi; "
    "or rdi, rax; cvtsi2sd xmm0, rdi; addsd xmm0, xmm0; 2:";
static char f32_i8[] = "cvttss2si eax, xmm0; movsx eax, al";
static char f32_i16[] = "cvttss2si eax, xmm0; movsx eax, ax";
static char f32_i32[] = "cvttss2si eax, xmm0";
static char f32_i64[] = "cvttss2si rax, xmm0";
static char f32_u8[] = "cvttss2si eax, xmm0; movzx eax, al";
static char f32_u16[] = "cvttss2si eax, xmm0; movzx eax, ax";
static char f32_u32[] = "cvttss2si rax, xmm0";
static char f32_u64[] = "cvttss2si rax, xmm0";
static char f32_f64[] = "cvtss2sd xmm0, xmm0";
static char f64_i8[] = "cvttsd2si eax, xmm0; movsx eax, al";
static char f64_i16[] = "cvttsd2si eax, xmm0; movsx eax, ax";
static char f64_i32[] = "cvttsd2si eax, xmm0";
static char f64_i64[] = "cvttsd2si rax, xmm0";
static char f64_u8[] = "cvttsd2si eax, xmm0; movzx eax, al";
static char f64_u16[] = "cvttsd2si eax, xmm0; movzx eax, ax";
static char f64_u32[] = "cvttsd2si rax, xmm0";
static char f64_u64[] = "cvttsd2si rax, xmm0";
static char f64_f32[] = "cvtsd2ss xmm0, xmm0";
static char* cast_table[][10] = {
  // to i8, i16, i32, i64, u8, u16, u32, u64, f32, f64
  {NULL, NULL, NULL, s32_64, z8_32, z16_32, NULL, s32_64, i32_f32, i32_f64},              // from i8
  {s8_32, NULL, NULL, s32_64, z8_32, z16_32, NULL, s32_64, i32_f32, i32_f64},             // from i16
  {s8_32, s16_32, NULL, s32_64, z8_32, z16_32, NULL, s32_64, i32_f32, i32_f64},           // from i32
  {s8_32, s16_32, NULL, NULL, z8_32, z16_32, NULL, NULL, i64_f32, i64_f64},               // from i64
  {s8_32, NULL, NULL, s32_64, NULL, NULL, NULL, s32_64, i32_f32, i32_f64},                // from u8
  {s8_32, s16_32, NULL, s32_64, z8_32, NULL, NULL, s32_64, i32_f32, i32_f64},             // from u16
  {s8_32, s16_32, NULL, z32_64, z8_32, z16_32, NULL, z32_64, u32_f32, u32_f64},           // from u32
  {s8_32, s16_32, NULL, NULL, z8_32, z16_32, NULL, NULL, u64_f32, u64_f64},               // from u64
  {f32_i8, f32_i16, f32_i32, f32_i64, f32_u8, f32_u16, f32_u32, f32_u64, NULL, f32_f64},  // from f32
  {f64_i8, f64_i16, f64_i32, f64_i64, f64_u8, f64_u16, f64_u32, f64_u64, f64_f32, NULL},  // from f64
};

enum { I8, I16, I32, I64, U8, U16, U32, U64, F32, F64 };

static int get_type_id(Type* type) {
  switch (type->kind) {
    case TY_CHAR:
      return type->is_unsigned ? U8 : I8;
    case TY_SHORT:
      return type->is_unsigned ? U16 : I16;
    case TY_INT:
      return type->is_unsigned ? U32 : I32;
    case TY_LONG:
      return type->is_unsigned ? U64 : I64;
    case TY_FLOAT:
      return F32;
    case TY_DOUBLE:
      return F64;
    default:
      return U64;
  }
}

static void gen_expr(Node* node);
static void gen_stmt(Node* node);

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

static void pushf(char* reg) {
  genln("  sub rsp, 8");
  genln("  movsd [rsp], %s", reg);
  depth++;
}

static void popf(char* reg) {
  genln("  movsd %s, [rsp]", reg);
  genln("  add rsp, 8");
  depth--;
}

static void popfn(int n) {
  char* reg = format("xmm%d", n);
  popf(reg);
}

static void push_composite_type(char* reg, Type* type) {
  if (type->kind != TY_STRUCT && type->kind != TY_UNION) {
    error("expected struct or union");
  }

  int size = align(type->size, 8);
  genln("  sub rsp, %d", size);
  depth += size / 8;

  for (int i = 0; i < type->size; i++) {
    genln("  mov r10b, [%s+%d]", reg, i);
    genln("  mov [rsp+%d], r10b", i);
  }
}

static void push_arg(Node* arg) {
  gen_expr(arg);
  switch (arg->type->kind) {
    case TY_STRUCT:
    case TY_UNION:
      push_composite_type("rax", arg->type);
      break;
    case TY_FLOAT:
    case TY_DOUBLE:
      pushf("xmm0");
      break;
    default:
      push("rax");
      break;
  }
}

static void push_passed_by_stack_args(Node* args) {
  if (!args) {
    return;
  }

  push_passed_by_stack_args(args->next);

  if (!args->is_passed_by_stack) {
    return;
  }

  push_arg(args);
}

static void push_passed_by_register_args(Node* args) {
  if (!args) {
    return;
  }

  push_passed_by_register_args(args->next);

  if (args->is_passed_by_stack) {
    return;
  }

  push_arg(args);
}

static bool have_float_data_at(Type* type, int low, int high, int offset) {
  switch (type->kind) {
    case TY_STRUCT:
    case TY_UNION:
      for (Member* mem = type->members; mem; mem = mem->next) {
        if (!have_float_data_at(mem->type, low, high, offset + mem->offset)) {
          return false;
        }
      }
      return true;
    case TY_ARRAY:
      for (int i = 0; i < type->len; i++) {
        if (!have_float_data_at(type->base, low, high, offset + type->base->size * i)) {
          return false;
        }
      }
      return true;
    default:
      return is_float(type) || offset < low || offset >= high;
  }
}

static bool have_float_data(Type* type, int low, int high) {
  return have_float_data_at(type, low, high, 0);
}

static int push_args(Node* args) {
  int stacked = 0;
  int int_cnt = 0;
  int float_cnt = 0;
  for (Node* arg = args; arg; arg = arg->next) {
    switch (arg->type->kind) {
      case TY_STRUCT:
      case TY_UNION:
        // Values whose types are struct or union are passed by registers
        // if their size is less than or equal to 16 bytes.
        // The size of a piece of stack is 8 bytes, so use registers up to 2.
        // Pass the values by floating point registers (XMM) when only floating point data reside
        // at their first or last 8 bytes, otherwise pass them by general-purpose registers
        if (arg->type->size > 16) {
          arg->is_passed_by_stack = true;
          stacked += align(arg->type->size, 8) / 8;
          continue;
        }

        bool former_float = have_float_data(arg->type, 0, 8);
        bool latter_float = have_float_data(arg->type, 8, 16);
        if (int_cnt + !former_float + !latter_float > MAX_INT_REG_ARGS
            || float_cnt + former_float + latter_float > MAX_FLOAT_REG_ARGS) {
          arg->is_passed_by_stack = true;
          stacked += align(arg->type->size, 8) / 8;
          continue;
        }

        int_cnt += !former_float + !latter_float;
        float_cnt += former_float + latter_float;
        continue;
      case TY_FLOAT:
      case TY_DOUBLE:
        if (++float_cnt > MAX_FLOAT_REG_ARGS) {
          arg->is_passed_by_stack = true;
          stacked++;
        }
        continue;
      default:
        if (++int_cnt > MAX_INT_REG_ARGS) {
          arg->is_passed_by_stack = true;
          stacked++;
        }
    }
  }

  // A value which is pushed in stack takes 8 bytes,
  // which means that the stack pointer is aligned 16 bytes boundary
  // when the depth is even
  if ((depth + stacked) % 2 == 1) {
    genln("  sub rsp, 8");
    stacked++;
    depth++;
  }

  // Push passed-by-stack args first, then
  // push passed-by-register args
  // in order not to pop passed-by-stack args
  push_passed_by_stack_args(args);
  push_passed_by_register_args(args);

  return stacked;
}

static int count_label(void) {
  static int count = 0;
  return count++;
}

static void cmp_zero(Type* type) {
  switch (type->kind) {
    case TY_FLOAT:
      genln("  xorps xmm1, xmm1");
      genln("  ucomiss xmm0, xmm1");
      return;
    case TY_DOUBLE:
      genln("  xorpd xmm1, xmm1");
      genln("  ucomisd xmm0, xmm1");
      return;
    default:
      if (is_integer(type) && type->size <= 4) {
        genln("  cmp eax, 0");
        return;
      }
      genln("  cmp rax, 0");
  }
}

static void load(Node* node) {
  switch (node->type->kind) {
    case TY_STRUCT:
    case TY_UNION:
    case TY_FUNC:
    case TY_ARRAY:
      return;
    case TY_FLOAT:
      genln("  movss xmm0, [rax]");
      return;
    case TY_DOUBLE:
      genln("  movsd xmm0, [rax]");
      return;
    default: {
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
  }
}

static void cast(Type* to, Type* from) {
  switch (to->kind) {
    case TY_VOID:
      return;
    case TY_BOOL:
      cmp_zero(from);
      genln("  setne al");
      genln("  movzx eax, al");
      return;
    default: {
      int from_id = get_type_id(from);
      int to_id = get_type_id(to);
      char* ins = cast_table[from_id][to_id];
      if (ins) {
        genln("  %s", ins);
      }
    }
  }
}

static void gen_addr(Node* node) {
  switch (node->kind) {
    case ND_COMMA:
      gen_expr(node->lhs);
      gen_addr(node->rhs);
      break;
    case ND_DEREF:
      gen_expr(node->lhs);
      break;
    case ND_FUNC:
      if (node->is_definition) {
        genln("  lea rax, %s[rip]", node->name);
      } else {
        genln("  mov rax, %s@GOTPCREL[rip]", node->name);
      }
      break;
    case ND_GVAR:
      genln("  lea rax, %s[rip]", node->name);
      break;
    case ND_LVAR:
      genln("  mov rax, rbp");
      genln("  sub rax, %d", node->obj->offset);
      break;
    case ND_MEMBER:
      gen_addr(node->lhs);
      genln("  add rax, %d", node->mem->offset);
      break;
    default:
      error_token(node->token, "expected a left value");
  }
}

static void gen_expr(Node* node) {
  genln("  .loc %d %d", node->token->file->index, node->token->line);

  switch (node->kind) {
    case ND_ASSIGN:
      gen_addr(node->lhs);
      push("rax");
      gen_expr(node->rhs);
      pop("rdi");
      switch (node->type->kind) {
        case TY_STRUCT:
        case TY_UNION:
          for (int i = 0; i < node->type->size; i++) {
            genln("  mov r8b, %d[rax]", i);
            genln("  mov %d[rdi], r8b", i);
          }
          return;
        case TY_FLOAT:
          genln("  movss [rdi], xmm0");
          return;
        case TY_DOUBLE:
          genln("  movsd [rdi], xmm0");
          return;
        default:
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
      }
    case ND_COMMA:
      gen_expr(node->lhs);
      gen_expr(node->rhs);
      return;
    case ND_COND: {
      int label = count_label();
      gen_expr(node->cond);
      cmp_zero(node->cond->type);
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
      switch (node->type->kind) {
        case TY_FLOAT:
          genln("  mov rax, 1");
          genln("  shl rax, 31");
          genln("  movq xmm1, rax");
          genln("  xorps xmm0, xmm1");
          return;
        case TY_DOUBLE:
          genln("  mov rax, 1");
          genln("  shl rax, 63");
          genln("  movq xmm1, rax");
          genln("  xorpd xmm0, xmm1");
        default:
          genln("  neg rax");
          return;
      }
    case ND_ADDR:
      gen_addr(node->lhs);
      return;
    case ND_DEREF:
      gen_expr(node->lhs);
      load(node);
      return;
    case ND_NOT:
      gen_expr(node->lhs);
      cmp_zero(node->lhs->type);
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
    case ND_FUNC:
    case ND_GVAR:
    case ND_LVAR:
    case ND_MEMBER:
      gen_addr(node);
      load(node);
      return;
    case ND_FUNCCALL: {
      // The number of argments which remain in stack
      // following x8664 psAPI
      // Those arguments which remain in stack are pushed
      // after aligning the stack pointer to 16 bytes boundary
      // so it is not necessary to align it again on call
      int stacked = push_args(node->args);

      // node->lhs may be another function call, which means that
      // it may use arguments registers for its arguments,
      // so resolve the address of function first
      // keeping the values of the arugments of this function call in stack
      // and then pop them, otherwise the poped values of the arguments of this function call
      // will be overridden
      gen_expr(node->lhs);

      int int_cnt = 0;
      int float_cnt = 0;
      for (Node* arg = node->args; arg; arg = arg->next) {
        switch (arg->type->kind) {
          case TY_STRUCT:
          case TY_UNION:
            if (arg->type->size > 16) {
              continue;
            }

            bool former_float = have_float_data(arg->type, 0, 8);
            bool latter_float = have_float_data(arg->type, 8, 16);
            if (int_cnt + !former_float + !latter_float > MAX_INT_REG_ARGS
                || float_cnt + former_float + latter_float > MAX_FLOAT_REG_ARGS) {
              continue;
            }

            if (former_float) {
              popfn(float_cnt++);
            } else {
              pop(arg_regs64[int_cnt++]);
            }
            if (arg->type->size > 8) {
              if (latter_float) {
                popfn(float_cnt++);
              } else {
                pop(arg_regs64[int_cnt++]);
              }
            }
            continue;
          case TY_FLOAT:
          case TY_DOUBLE:
            if (float_cnt < MAX_FLOAT_REG_ARGS) {
              popfn(float_cnt++);
            }
            continue;
          default:
            if (int_cnt < MAX_INT_REG_ARGS) {
              pop(arg_regs64[int_cnt++]);
            }
            continue;
        }
      }

      genln("  mov r10, rax");
      genln("  mov rax, %d", float_cnt);
      genln("  call r10");
      genln("  add rsp, %d", 8 * stacked);

      depth -= stacked;

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
    case ND_NUM: {
      union {
        float f32;
        double f64;
        uint32_t u32;
        uint64_t u64;
      } val;

      switch (node->type->kind) {
        case TY_FLOAT:
          val.f32 = node->float_val;
          genln("  mov eax, %u # float %f", val.u32, val.f32);
          genln("  movq xmm0, rax");
          return;
        case TY_DOUBLE:
          val.f64 = node->float_val;
          genln("  mov rax, %lu # double %f", val.u64, val.f64);
          genln("  movq xmm0, rax");
          return;
        default:
          genln("  mov rax, %ld", node->int_val);
          return;
      }
    }
    case ND_NULL:
      return;
    case ND_MEMZERO:
      genln("  mov rcx, %d", node->type->size);
      genln("  mov rdi, rbp");
      genln("  sub rdi, %d", node->obj->offset);
      genln("  mov al, 0");
      genln("  rep stosb");
      return;
    case ND_OR: {
      int label = count_label();
      gen_expr(node->lhs);
      cmp_zero(node->lhs->type);
      genln("  jne .Ltrue%d", label);
      gen_expr(node->rhs);
      cmp_zero(node->rhs->type);
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
      cmp_zero(node->lhs->type);
      genln("  je .Lfalse%d", label);
      gen_expr(node->rhs);
      cmp_zero(node->rhs->type);
      genln("  je .Lfalse%d", label);
      genln("  mov rax, 1");
      genln("  jmp .Lend%d", label);
      genln(".Lfalse%d:", label);
      genln("  mov rax, 0");
      genln(".Lend%d:", label);
      return;
    }
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

  if (is_float(node->lhs->type)) {
    gen_expr(node->rhs);
    pushf("xmm0");
    gen_expr(node->lhs);
    popf("xmm1");

    char* size = node->lhs->type->kind == TY_FLOAT ? "ss" : "sd";

    switch (node->kind) {
      case ND_OR:
      case ND_AND: {
        break;
      }
      case ND_EQ:
        genln("  ucomi%s xmm1, xmm0", size);
        genln("  sete al");
        genln("  setnp dl");
        genln("  and al, dl");
        genln("  and al, 1");
        genln("  movzx rax, al");
        return;
      case ND_NE:
        genln("  ucomi%s xmm1, xmm0", size);
        genln("  setne al");
        genln("  setp dl");
        genln("  or al, dl");
        genln("  and al, 1");
        genln("  movzx rax, al");
        return;
      case ND_LT:
        genln("  ucomi%s xmm1, xmm0", size);
        genln("  seta al");
        genln("  and al, 1");
        genln("  movzx rax, al");
        return;
      case ND_LE:
        genln("  ucomi%s xmm1, xmm0", size);
        genln("  setae al");
        genln("  and al, 1");
        genln("  movzx rax, al");
        return;
      case ND_ADD:
        genln("  add%s xmm0, xmm1", size);
        return;
      case ND_SUB:
        genln("  sub%s xmm0, xmm1", size);
        return;
      case ND_MUL:
        genln("  mul%s xmm0, xmm1", size);
        return;
      case ND_DIV:
        genln("  div%s xmm0, xmm1", size);
        return;
      default:
        error_token(node->token, "invalid expression");
        return;
    }
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
  genln("  .loc %d %d", node->token->file->index, node->token->line);

  switch (node->kind) {
    case ND_BLOCK:
      for (Node* body = node->body; body; body = body->next) {
        gen_stmt(body);
      }
      return;
    case ND_IF: {
      int lelse = count_label();
      int lend = count_label();

      gen_expr(node->cond);
      cmp_zero(node->cond->type);
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
        genln("  cmp %s, %ld", r, c->int_val);
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
        cmp_zero(node->cond->type);
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

static void gen_data(TopLevelObj* codes) {
  for (TopLevelObj* var = codes; var; var = var->next) {
    if (var->obj->kind != OJ_GVAR) {
      continue;
    }
    if (!var->obj->is_definition) {
      continue;
    }

    if (var->obj->is_static) {
      genln(".local %s", var->obj->name);
    } else {
      genln(".global %s", var->obj->name);
    }

    if (var->obj->val) {
      genln(".data");
      genln(".align %d", var->obj->alignment);
      genln("%s:", var->obj->name);

      Relocation* reloc = var->obj->relocs;
      int offset = 0;
      while (offset < var->obj->type->size) {
        if (reloc && offset == reloc->offset) {
          genln("  .quad %s%+ld", reloc->label, reloc->addend);
          reloc = reloc->next;
          offset += 8;
          continue;
        }

        genln("  .byte %d", var->obj->val[offset++]);
      }

      continue;
    }

    genln(".bss");
    genln(".align %d", var->obj->alignment);
    genln("%s:", var->obj->name);
    genln("  .zero %d", var->obj->type->size);
  }
}

static void store_via_int_reg(int size, int offset, int n) {
  switch (size) {
    case 1:
      genln("  mov [rax+%d], %s", offset, arg_regs8[n]);
      break;
    case 2:
      genln("  mov [rax+%d], %s", offset, arg_regs16[n]);
      break;
    case 4:
      genln("  mov [rax+%d], %s", offset, arg_regs32[n]);
      break;
    default:
      genln("  mov [rax+%d], %s", offset, arg_regs64[n]);
      break;
  }
}

static void store_via_float_reg(int size, int offset, int n) {
  switch (size) {
    case 4:
      genln("  movss [rax+%d], xmm%d", offset, n);
      break;
    case 8:
      genln("  movsd [rax+%d], xmm%d", offset, n);
      break;
  }
}

static void gen_text(TopLevelObj* codes) {
  for (TopLevelObj* func = codes; func; func = func->next) {
    if (func->obj->kind != OJ_FUNC || !func->obj->is_definition) {
      continue;
    }

    genln(".text");
    if (func->obj->is_static) {
      genln(".local %s", func->obj->name);
    } else {
      genln(".global %s", func->obj->name);
    }

    genln("%s:", func->obj->name);
    push_outside_frame("rbp");
    genln("  mov rbp, rsp");
    genln("  sub rsp, %d", func->obj->stack_size);

    if (func->obj->va_area) {
      int int_cnt = 0;
      int float_cnt = 0;
      for (Node* param = func->obj->params; param; param = param->next) {
        if (is_float(param->type)) {
          float_cnt++;
        } else {
          int_cnt++;
        }
      }

      int offset = func->obj->va_area->offset;

      // to assign __va_area__ to __va_elem
      // set __va_area__ as __va_elem manually in memory
      // __va_area_.gp_offset (int)
      genln("  mov DWORD PTR [rbp - %d], %d", offset, int_cnt * 8);
      // __va_area_.fp_offset (int)
      genln("  mov DWORD PTR [rbp - %d], %d", offset - 4, float_cnt * 8 + 48);
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

    int int_cnt = 0;
    int float_cnt = 0;
    int offset = 16;
    for (Node* param = func->obj->params; param; param = param->next) {
      switch (param->type->kind) {
        case TY_STRUCT:
        case TY_UNION: {
          if (param->type->size > 16) {
            break;
          }

          bool former_float = have_float_data(param->type, 0, 8);
          bool latter_float = have_float_data(param->type, 8, 16);
          if (int_cnt + !former_float + !latter_float > MAX_INT_REG_ARGS
              || float_cnt + former_float + latter_float > MAX_FLOAT_REG_ARGS) {
            break;
          }

          int_cnt += !former_float + !latter_float;
          float_cnt += former_float + latter_float;
          continue;
        }
        case TY_FLOAT:
        case TY_DOUBLE:
          if (++float_cnt <= MAX_FLOAT_REG_ARGS) {
            continue;
          }
          break;
        default:
          if (++int_cnt <= MAX_INT_REG_ARGS) {
            continue;
          }
          break;
      }

      offset = align(offset, 8);
      param->obj->offset = -offset;
      offset += param->type->size;
    }

    int_cnt = 0;
    float_cnt = 0;
    for (Node* param = func->obj->params; param; param = param->next) {
      if (param->obj->offset < 0) {
        continue;
      }

      gen_addr(param);

      switch (param->type->kind) {
        case TY_STRUCT:
        case TY_UNION: {
          int size = param->type->size <= 8 ? param->type->size : 8;
          if (have_float_data(param->type, 0, 8)) {
            store_via_float_reg(size, 0, float_cnt++);
          } else {
            store_via_int_reg(size, 0, int_cnt++);
          }
          if (param->type->size > 8) {
            int size = param->type->size - 8;
            if (have_float_data(param->type, 8, 16)) {
              store_via_float_reg(size, 8, float_cnt++);
            } else {
              store_via_int_reg(size, 8, int_cnt++);
            }
          }
          break;
        }
        case TY_FLOAT:
        case TY_DOUBLE:
          store_via_float_reg(param->type->size, 0, float_cnt++);
          break;
        default:
          store_via_int_reg(param->type->size, 0, int_cnt++);
          break;
      }
    }

    gen_stmt(func->obj->body);

    genln(".Lreturn%d:", func_count++);
    genln("  mov rsp, rbp");
    pop_outside_frame("rbp");
    genln("  ret");
  }
}

static void assert_depth_offset(char* name, int depth) {
  if (depth != 0) {
    error("%s: push and pop do not offset each other: %d", name, depth);
  }
}

void gen(char* output_filename, TopLevelObj* codes) {
  output_file = open_output_file(output_filename);

  genln(".intel_syntax noprefix");
  for (File* file = files; file; file = file->next) {
    genln(".file %d \"%s\"", file->index, file->name);
  }
  gen_data(codes);
  gen_text(codes);

  fclose(output_file);

  assert_depth_offset("depth_outside_frame", depth_outside_frame);
  assert_depth_offset("depth", depth);
}