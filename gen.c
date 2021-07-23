#include "cc.h"

static FILE* output_file;

static int depth_outside_frame = 0;
static int depth = 0;

static Obj* current_func;
static int func_cnt = 0;

#define MAX_FLOAT_REG_ARGS 8
#define MAX_GENERAL_REG_ARGS 6

static char* arg_regs8[] = {"%dil", "%sil", "%dl", "%cl", "%r8b", "%r9b"};
static char* arg_regs16[] = {"%di", "%si", "%dx", "%cx", "%r8w", "%r9w"};
static char* arg_regs32[] = {"%edi", "%esi", "%edx", "%ecx", "%r8d", "%r9d"};
static char* arg_regs64[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};

static char s8_32[] = "movsx %al, %eax";
static char s16_32[] = "movsx %ax, %eax";
static char s32_64[] = "movsx %eax, %rax";
static char i32_f32[] = "cvtsi2ss %eax, %xmm0";
static char i32_f64[] = "cvtsi2sd %eax, %xmm0";
static char i32_f80[] = "mov %eax, -4(%rsp); fildl -4(%rsp)";
static char i64_f32[] = "cvtsi2ss %rax, %xmm0";
static char i64_f64[] = "cvtsi2sd %rax, %xmm0";
static char i64_f80[] = "movq %rax, -8(%rsp); fildll -8(%rsp)";
static char z8_32[] = "movzx %al, %eax";
static char z16_32[] = "movzx %ax, %eax";
static char z32_64[] = "mov %eax, %eax";
static char u32_f32[] = "mov %eax, %eax; cvtsi2ss %rax, %xmm0";
static char u32_f64[] = "mov %eax, %eax; cvtsi2sd %rax, %xmm0";
static char u32_f80[] = "mov %eax, %eax; mov %rax, -8(%rsp); fildll -8(%rsp)";
static char u64_f32[] = "cvtsi2ss %rax, %xmm0";
static char u64_f64[]
  = "test %rax, %rax; js 1f; pxor %xmm0, %xmm0; cvtsi2sd %rax, %xmm0; jmp 2f; "
    "1: mov %rax, %rdi; and $1, %eax; pxor %xmm0, %xmm0; shr %rdi; "
    "or %rax, %rdi; cvtsi2sd %rdi, %xmm0; addsd %xmm0, %xmm0; 2:";
static char u64_f80[]
  = "mov %rax, -8(%rsp); fildq -8(%rsp); test %rax, %rax; jns 1f;"
    "mov $1602224128, %eax; mov %eax, -4(%rsp); fadds -4(%rsp); 1:";
static char f32_i8[] = "cvttss2sil %xmm0, %eax; movsx %al, %eax";
static char f32_i16[] = "cvttss2sil %xmm0, %eax; movsx %ax, %eax";
static char f32_i32[] = "cvttss2sil %xmm0, %eax";
static char f32_i64[] = "cvttss2siq %xmm0, %rax";
static char f32_u8[] = "cvttss2sil %xmm0, %eax; movzx %al, %eax";
static char f32_u16[] = "cvttss2sil %xmm0, %eax; movzx %ax, %eax";
static char f32_u32[] = "cvttss2siq %xmm0, %rax";
static char f32_u64[] = "cvttss2siq %xmm0, %rax";
static char f32_f64[] = "cvtss2sd %xmm0, %xmm0";
static char f32_f80[] = "movss %xmm0, -4(%rsp); flds -4(%rsp)";
static char f64_i8[] = "cvttsd2sil %xmm0, %eax; movsx %al, %eax";
static char f64_i16[] = "cvttsd2sil %xmm0, %eax; movsx %ax, %eax";
static char f64_i32[] = "cvttsd2sil %xmm0, %eax";
static char f64_i64[] = "cvttsd2siq %xmm0, %rax";
static char f64_u8[] = "cvttsd2sil %xmm0, %eax; movzx %al, %eax";
static char f64_u16[] = "cvttsd2sil %xmm0, %eax; movzx %ax, %eax";
static char f64_u32[] = "cvttsd2siq %xmm0, %rax";
static char f64_u64[] = "cvttsd2siq %xmm0, %rax";
static char f64_f32[] = "cvtsd2ss %xmm0, %xmm0";
static char f64_f80[] = "movsd %xmm0, -8(%rsp); fldl -8(%rsp)";

#define FROM_F80_1 "fnstcw -10(%rsp); movzwl -10(%rsp), %eax; or $12, %ah; mov %ax, -12(%rsp); fldcw -12(%rsp);"
#define FROM_F80_2 " -24(%rsp); fldcw -10(%rsp); "

static char f80_i8[] = FROM_F80_1 "fistps" FROM_F80_2 "movsbl -24(%rsp), %eax";
static char f80_i16[] = FROM_F80_1 "fistps" FROM_F80_2 "movzbl -24(%rsp), %eax";
static char f80_i32[] = FROM_F80_1 "fistpl" FROM_F80_2 "mov -24(%rsp), %eax";
static char f80_i64[] = FROM_F80_1 "fistpq" FROM_F80_2 "mov -24(%rsp), %rax";
static char f80_u8[] = FROM_F80_1 "fistps" FROM_F80_2 "movzbl -24(%rsp), %eax";
static char f80_u16[] = FROM_F80_1 "fistpl" FROM_F80_2 "movzwl -24(%rsp), %eax";
static char f80_u32[] = FROM_F80_1 "fistpl" FROM_F80_2 "mov -24(%rsp), %eax";
static char f80_u64[] = FROM_F80_1 "fistpq" FROM_F80_2 "mov -24(%rsp), %rax";
static char f80_f32[] = "fstps -8(%rsp); movss -8(%rsp), %xmm0";
static char f80_f64[] = "fstpl -8(%rsp); movsd -8(%rsp), %xmm0";
static char* cast_table[][11] = {
  // to i8, i16, i32, i64, u8, u16, u32, u64, f32, f64, f80
  {NULL, NULL, NULL, s32_64, z8_32, z16_32, NULL, s32_64, i32_f32, i32_f64, i32_f80},              // from i8
  {s8_32, NULL, NULL, s32_64, z8_32, z16_32, NULL, s32_64, i32_f32, i32_f64, i32_f80},             // from i16
  {s8_32, s16_32, NULL, s32_64, z8_32, z16_32, NULL, s32_64, i32_f32, i32_f64, i32_f80},           // from i32
  {s8_32, s16_32, NULL, NULL, z8_32, z16_32, NULL, NULL, i64_f32, i64_f64, i64_f80},               // from i64
  {s8_32, NULL, NULL, s32_64, NULL, NULL, NULL, s32_64, i32_f32, i32_f64, i32_f80},                // from u8
  {s8_32, s16_32, NULL, s32_64, z8_32, NULL, NULL, s32_64, i32_f32, i32_f64, i32_f80},             // from u16
  {s8_32, s16_32, NULL, z32_64, z8_32, z16_32, NULL, z32_64, u32_f32, u32_f64, u32_f80},           // from u32
  {s8_32, s16_32, NULL, NULL, z8_32, z16_32, NULL, NULL, u64_f32, u64_f64, u64_f80},               // from u64
  {f32_i8, f32_i16, f32_i32, f32_i64, f32_u8, f32_u16, f32_u32, f32_u64, NULL, f32_f64, f32_f80},  // from f32
  {f64_i8, f64_i16, f64_i32, f64_i64, f64_u8, f64_u16, f64_u32, f64_u64, f64_f32, NULL, f64_f80},  // from f64
  {f80_i8, f80_i16, f80_i32, f80_i64, f80_u8, f80_u16, f80_u32, f80_u64, f80_f32, f80_f64, NULL},  // from f80
};

enum { I8, I16, I32, I64, U8, U16, U32, U64, F32, F64, F80 };

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
    case TY_LDOUBLE:
      return F80;
    default:
      return U64;
  }
}

static char* reg_ax(int size) {
  switch (size) {
    case 1:
      return "%al";
    case 2:
      return "%ax";
    case 4:
      return "%eax";
    default:
      return "%rax";
  }
}

static char* reg_dx(int size) {
  switch (size) {
    case 1:
      return "%dl";
    case 2:
      return "%dx";
    case 4:
      return "%edx";
    default:
      return "%rdx";
  }
}

static void gen_expr(Node* node);
static void gen_stmt(Node* node);

__attribute__((format(printf, 1, 2))) static void genln(char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(output_file, fmt, args);
  fprintf(output_file, "\n");
}

static void gen_addr(Node* node) {
  switch (node->kind) {
    case ND_ASSIGN:
    case ND_COND:
      if (!is_composite_type(node->type)) {
        break;
      }

      gen_expr(node);
      break;
    case ND_COMMA:
      gen_expr(node->lhs);
      gen_addr(node->rhs);
      break;
    case ND_DEREF:
      gen_expr(node->lhs);
      break;
    case ND_FUNC:
      if (as_pic) {
        genln("  mov %s@GOTPCREL(%%rip), %%rax", node->obj->name);
        break;
      }

      if (node->obj->is_definition) {
        genln("  lea %s(%%rip), %%rax", node->obj->name);
      } else {
        genln("  mov %s@GOTPCREL(%%rip), %%rax", node->obj->name);
      }
      break;
    case ND_GVAR:
      if (as_pic) {
        if (node->obj->is_thread_local) {
          genln("  data16 lea %s@TLSGD(%%rip), %%rdi", node->obj->name);
          genln("  .value $0x6666");
          genln("  rex64");
          genln("  call __tls_get_addr@PLT");
          break;
        }

        genln("  mov %s@GOTPCREL(%%rip), %%rax", node->obj->name);
        break;
      }

      if (node->obj->is_thread_local) {
        genln("  lea %s@TPOFF, %%rax", node->obj->name);
        genln("  add %%fs:0, %%rax");
        break;
      }
      genln("  lea %s(%%rip), %%rax", node->obj->name);
      break;
    case ND_LVAR:
      // The address of VLA is stored in this variable unlike array, so load it.
      if (node->type->kind == TY_VL_ARRAY) {
        genln("  mov %d(%%rbp), %%rax", -1 * node->obj->offset);
      } else {
        genln("  lea %d(%%rbp), %%rax", -1 * node->obj->offset);
      }
      break;
    case ND_MEMBER:
      gen_addr(node->lhs);
      genln("  add $%d, %%rax", node->mem->offset);
      break;
    case ND_FUNCCALL:
      if (node->return_val) {
        gen_expr(node);
      }
      break;
    default:
      UNREACHABLE("expected a left value but got %d", node->kind);
  }
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
  genln("  sub $8, %%rsp");
  genln("  movsd %s, (%%rsp)", reg);
  depth++;
}

static void popf(char* reg) {
  genln("  movsd (%%rsp), %s", reg);
  genln("  add $8, %%rsp");
  depth--;
}

static void popfn(int n) {
  char* reg = format("%%xmm%d", n);
  popf(reg);
}

static void push_composite_type(char* reg, Type* type) {
  if (!is_composite_type(type)) {
    UNREACHABLE("expected a composite typed value but got %d", type->kind);
  }

  int size = align_up(type->size, 8);
  genln("  sub $%d, %%rsp", size);
  depth += size / 8;

  for (int i = 0; i < type->size; i++) {
    genln("  mov %d(%s), %%r10b", i, reg);
    genln("  mov %%r10b, %d(%%rsp)", i);
  }
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
      return type->kind == TY_FLOAT || type->kind == TY_DOUBLE || offset < low || offset >= high;
  }
}

static bool have_float_data(Type* type, int low, int high) {
  return have_float_data_at(type, low, high, 0);
}

static int count_label(void) {
  static int count = 0;
  return count++;
}

static void cmp_zero(Type* type) {
  switch (type->kind) {
    case TY_FLOAT:
      genln("  xorps %%xmm1, %%xmm1");
      genln("  ucomiss %%xmm1, %%xmm0");
      return;
    case TY_DOUBLE:
      genln("  xorpd %%xmm1, %%xmm1");
      genln("  ucomisd %%xmm1, %%xmm0");
      return;
    case TY_LDOUBLE:
      genln("  fldz");
      genln("  fucomip");
      genln("  fstp %%st(0)");
      return;
    default:
      if (is_int_type(type) && type->size <= 4) {
        genln("  cmp $0, %%eax");
        return;
      }

      genln("  cmp $0, %%rax");
  }
}

static void load(Node* node) {
  switch (node->type->kind) {
    case TY_STRUCT:
    case TY_UNION:
    case TY_FUNC:
    case TY_ARRAY:
    case TY_VL_ARRAY:
      return;
    case TY_FLOAT:
      genln("  movss (%%rax), %%xmm0");
      return;
    case TY_DOUBLE:
      genln("  movsd (%%rax), %%xmm0");
      return;
    case TY_LDOUBLE:
      genln("  fldt (%%rax)");
      return;
    default: {
      char* ins = node->type->is_unsigned ? "movz" : "movs";
      switch (node->type->size) {
        case 1:
          genln("  %sbl (%%rax), %%eax", ins);
          return;
        case 2:
          genln("  %swl (%%rax), %%eax", ins);
          return;
        case 4:
          genln("  movsxd (%%rax), %%rax");
          return;
        default:
          genln("  mov (%%rax), %%rax");
          return;
      }
    }
  }
}

static void store(Node* node) {
  switch (node->type->kind) {
    case TY_STRUCT:
    case TY_UNION:
      for (int i = 0; i < node->type->size; i++) {
        genln("  mov %d(%%rax), %%r8b", i);
        genln("  mov %%r8b, %d(%%rdi)", i);
      }
      return;
    case TY_FLOAT:
      genln("  movss %%xmm0, (%%rdi)");
      return;
    case TY_DOUBLE:
      genln("  movsd %%xmm0, (%%rdi)");
      return;
    case TY_LDOUBLE:
      genln("  fstpt (%%rdi)");
      return;
    default:
      genln("  mov %s, (%%rdi)", reg_ax(node->type->size));
  }
}

static void cast(Type* to, Type* from) {
  switch (to->kind) {
    case TY_VOID:
      return;
    case TY_BOOL:
      cmp_zero(from);
      genln("  setne %%al");
      genln("  movzx %%al, %%eax");
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

static void gen_assign(Node* node) {
  gen_addr(node->lhs);
  push("%rax");
  gen_expr(node->rhs);

  if (node->lhs->kind != ND_MEMBER || !node->lhs->mem->is_bitfield) {
    pop("%rdi");
    store(node);
    return;
  }

  // When the assignee (node->lhs) is the member of a compsite type and bitfield,
  // calculate the value which should be the result of this assignment in the node->rhs
  // so that this assign expression can do the assignment of a value of the composite type as normally.
  Member* mem = node->lhs->mem;
  genln("  mov %%rax, %%r8");  // Keep the node->rhs value as the last expression

  // The bitfield value of the member
  genln("  mov %%rax, %%rdi");
  genln("  and $%ld, %%rdi", (1L << mem->bit_width) - 1);
  genln("  shl $%d, %%rdi", mem->bit_offset);

  // Get the value of the node->lhs without popping stack.
  genln("  mov (%%rsp), %%rax");
  load(node->lhs);
  // Merge the node->lhs value of the composite type with the bitfield value.
  int mask = ((1L << mem->bit_width) - 1) << mem->bit_offset;
  genln("  mov $%d, %%r9", ~mask);
  genln("  and %%r9, %%rax");
  genln("  or %%rdi, %%rax");

  pop("%rdi");
  store(node);

  genln("  mov %%r8, %%rax");
}

static void gen_cond(Node* node) {
  int label = count_label();

  gen_expr(node->cond);
  cmp_zero(node->cond->type);

  genln("  je .Lelse%d", label);

  gen_expr(node->then);

  genln("  jmp .Lend%d", label);

  genln(".Lelse%d:", label);
  gen_expr(node->els);

  genln(".Lend%d:", label);
}

static void gen_neg(Node* node) {
  gen_expr(node->lhs);
  switch (node->type->kind) {
    case TY_FLOAT:
      genln("  mov $1, %%rax");
      genln("  shl $31, %%rax");
      genln("  movq %%rax, %%xmm1");
      genln("  xorps %%xmm1, %%xmm0");
      return;
    case TY_DOUBLE:
      genln("  mov $1, %%rax");
      genln("  shl $63, %%rax");
      genln("  movq %%rax, %%xmm1");
      genln("  xorpd %%xmm1, %%xmm0");
      return;
    case TY_LDOUBLE:
      genln("  fchs");
      return;
    default:
      genln("  neg %%rax");
      return;
  }
}

static void push_arg(Node* arg) {
  gen_expr(arg);
  switch (arg->type->kind) {
    case TY_STRUCT:
    case TY_UNION:
      push_composite_type("%rax", arg->type);
      break;
    case TY_FLOAT:
    case TY_DOUBLE:
      pushf("%xmm0");
      break;
    case TY_LDOUBLE:
      genln("  sub $16, %%rsp");
      genln("  fstpt (%%rsp)");
      depth += 2;
      break;
    default:
      push("%rax");
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

static void push_passed_by_reg_args(Node* args) {
  if (!args) {
    return;
  }

  push_passed_by_reg_args(args->next);

  if (args->is_passed_by_stack) {
    return;
  }

  push_arg(args);
}

static int push_args(Node* node) {
  int stacked = 0;
  int general_cnt = 0;
  int float_cnt = 0;

  // When the return v%%alues of function c%%alls are struct or union and their size are larger than
  // 16 bytes, the pointers to the v%%alues are passed as if they are the first arguments
  // for the c%%allee to fill the return v%%alues to the pointers.
  if (node->return_val && node->type->size > 16) {
    general_cnt++;
  }

  for (Node* arg = node->args; arg; arg = arg->next) {
    switch (arg->type->kind) {
      case TY_STRUCT:
      case TY_UNION:
        // V%%alues whose types are struct or union are passed by registers
        // if their size is less than or equ%%al to 16 bytes.
        // The size of a piece of stack is 8 bytes, so use registers up to 2.
        // Pass the v%%alues by floating point registers (XMM) when only floating point data reside
        // at their first or last 8 bytes, otherwise pass them by gener%%al-purpose registers
        if (arg->type->size > 16) {
          arg->is_passed_by_stack = true;
          stacked += align_up(arg->type->size, 8) / 8;
          continue;
        }

        bool former_float = have_float_data(arg->type, 0, 8);
        bool latter_float = have_float_data(arg->type, 8, 16);
        if (general_cnt + !former_float + !latter_float > MAX_GENERAL_REG_ARGS
            || float_cnt + former_float + latter_float > MAX_FLOAT_REG_ARGS) {
          arg->is_passed_by_stack = true;
          stacked += align_up(arg->type->size, 8) / 8;
          continue;
        }

        general_cnt += !former_float + !latter_float;
        float_cnt += former_float + latter_float;
        continue;
      case TY_FLOAT:
      case TY_DOUBLE:
        if (++float_cnt > MAX_FLOAT_REG_ARGS) {
          arg->is_passed_by_stack = true;
          stacked++;
        }
        continue;
      case TY_LDOUBLE:
        arg->is_passed_by_stack = true;
        stacked += 2;
        continue;
      default:
        if (++general_cnt > MAX_GENERAL_REG_ARGS) {
          arg->is_passed_by_stack = true;
          stacked++;
        }
    }
  }

  // A value which is pushed in stack takes 8 bytes,
  // which means that the stack pointer is %%aligned 16 bytes boundary
  // when the depth is even
  if ((depth + stacked) % 2 == 1) {
    genln("  sub $8, %%rsp");
    stacked++;
    depth++;
  }

  // Push passed-by-stack args first, then
  // push passed-by-register args
  // in order not to pop passed-by-stack args
  push_passed_by_stack_args(node->args);
  push_passed_by_reg_args(node->args);

  if (node->return_val && node->type->size > 16) {
    gen_addr(node->return_val);
    push("%rax");
  }

  return stacked;
}

static bool equal_to_func(Node* node, char* func) {
  return node->kind == ND_FUNC && equal_to_str(node->obj->name, func);
}

static void gen_alloca(Node* node) {
  gen_expr(node->args);
  genln("  mov %%rax, %%rdi");

  // align the arg (the size to %%allocate) to 16 byte boundary.
  genln("  add $15, %%rdi");
  genln("  and $0xfffffff0, %%edi");

  // Copy v%%alues pushed on stack at this time and keep having them at the top of stack
  // so that those v%%alues can be poped, and leave space for the size to %%allocate.
  genln("  mov %d(%%rbp), %%rcx", -1 * current_func->alloca_bottom->offset);
  genln("  sub %%rsp, %%rcx");
  genln("  mov %%rsp, %%rax");
  genln("  sub %%rdi, %%rsp");
  genln("  mov %%rsp, %%rdx");
  genln("1:");
  genln("  cmp $0, %%rcx");
  genln("  je  2f");
  genln("  mov (%%rax), %%r8b");
  genln("  mov %%r8b, (%%rdx)");
  genln("  inc %%rdx");
  genln("  inc %%rax");
  genln("  dec %%rcx");
  genln("  jmp 1b");
  genln("2:");

  // Use the left space for the %%alloca value and keep it for the next %%alloca.
  genln("  mov %d(%%rbp), %%rax", -1 * current_func->alloca_bottom->offset);
  genln("  sub %%rdi, %%rax");
  genln("  mov %%rax, %d(%%rbp)", -1 * current_func->alloca_bottom->offset);
}

static void store_returned_by_reg_val(Node* node) {
  int general_cnt = 0;
  int float_cnt = 0;

  int size = MIN(node->type->size, 8);
  if (have_float_data(node->type, 0, 8)) {
    switch (size) {
      case 4:
        genln("  movss %%xmm0, %d(%%rbp)", -1 * node->obj->offset);
        break;
      case 8:
        genln("  movsd %%xmm0, %d(%%rbp)", -1 * node->obj->offset);
        break;
    }
    float_cnt++;
  } else {
    for (int i = 0; i < size; i++) {
      genln("  mov %%al, %d(%%rbp)", -1 * (node->obj->offset - i));
      genln("  shr $8, %%rax");
    }
    general_cnt++;
  }

  if (node->type->size > 8) {
    size = node->type->size - 8;
    if (have_float_data(node->type, 8, 16)) {
      switch (size) {
        case 4:
          genln("  movss %%xmm%d, %d(%%rbp)", float_cnt, -1 * (node->obj->offset - 8));
          return;
        case 8:
          genln("  movsd %%xmm%d, %d(%%rbp)", float_cnt, -1 * (node->obj->offset - 8));
          return;
      }
    } else {
      char* reg1 = general_cnt == 0 ? "%al" : "%dl";
      char* reg2 = general_cnt == 0 ? "%rax" : "%rdx";
      for (int i = 0; i < size; i++) {
        genln("  mov %s, %d(%%rbp)", reg1, -1 * (node->obj->offset - 8 - i));
        genln("  shr $8, %s", reg2);
      }
    }
  }
}

static void gen_funccall(Node* node) {
  if (equal_to_func(node->lhs, "alloca")) {
    gen_alloca(node);
    return;
  }

  // The number of argments which remain in stack
  // following x8664 psAPI.
  // Those arguments which remain in stack are pushed
  // after %%aligning the stack pointer to 16 bytes boundary
  // so it is not necessary to align it again on call.
  int stacked = push_args(node);

  // node->lhs may be another function call, which means that
  // it may use arguments registers for its arguments,
  // so resolve the address of function first
  // keeping the v%%alues of the arugments of this function call in stack
  // and then pop them, otherwise the poped v%%alues of the arguments of this function call
  // will be overridden.
  gen_expr(node->lhs);

  int general_cnt = 0;
  int float_cnt = 0;

  if (node->return_val && node->type->size > 16) {
    pop(arg_regs64[general_cnt++]);
  }

  for (Node* arg = node->args; arg; arg = arg->next) {
    switch (arg->type->kind) {
      case TY_STRUCT:
      case TY_UNION:
        if (arg->type->size > 16) {
          continue;
        }

        bool former_float = have_float_data(arg->type, 0, 8);
        bool latter_float = have_float_data(arg->type, 8, 16);
        if (general_cnt + !former_float + !latter_float > MAX_GENERAL_REG_ARGS
            || float_cnt + former_float + latter_float > MAX_FLOAT_REG_ARGS) {
          continue;
        }

        if (former_float) {
          popfn(float_cnt++);
        } else {
          pop(arg_regs64[general_cnt++]);
        }
        if (arg->type->size > 8) {
          if (latter_float) {
            popfn(float_cnt++);
          } else {
            pop(arg_regs64[general_cnt++]);
          }
        }
        continue;
      case TY_FLOAT:
      case TY_DOUBLE:
        if (float_cnt < MAX_FLOAT_REG_ARGS) {
          popfn(float_cnt++);
        }
        continue;
      case TY_LDOUBLE:
        continue;
      default:
        if (general_cnt < MAX_GENERAL_REG_ARGS) {
          pop(arg_regs64[general_cnt++]);
        }
        continue;
    }
  }

  genln("  mov %%rax, %%r10");
  genln("  mov $%d, %%rax", float_cnt);
  genln("  call *%%r10");
  genln("  add $%d, %%rsp", 8 * stacked);

  depth -= stacked;

  char* ins = node->type->is_unsigned ? "movz" : "movs";
  switch (node->type->kind) {
    case TY_BOOL:
      genln("  movzx %%al, %%eax");
      break;
    case TY_CHAR:
      genln("  %sx %%al, %%eax", ins);
      break;
    case TY_SHORT:
      genln("  %sx %%ax, %%eax", ins);
      break;
    default: {
    }
  }

  // The return v%%alues of function c%%alls whose types are struct or union and whose size are
  // less than or equ%%al to 16 bytes are returned by up to 2 registers.
  // So store them to the stack of the c%%aller assigned for the return v%%alues.
  if (node->return_val && node->type->size <= 16) {
    store_returned_by_reg_val(node->return_val);
    gen_addr(node->return_val);
  }
}

static void gen_atomic_cas(Node* node) {
  gen_expr(node->cas_addr);
  push("%rax");

  gen_expr(node->cas_new_val);
  push("%rax");

  gen_expr(node->cas_old_val);
  // The %%rax may be overridden by the cmpxhg below, so keep it somewhere.
  genln("  mov %%rax, %%r8");
  load(node->cas_old_val);

  pop("%rdx");
  pop("%rdi");

  int size = node->cas_addr->type->base->size;
  // This cmpxchg compares the value of the node->cas_addr and the node->cas_new,
  // and store the node->cas_new to the node->cas_addr if those are equ%%al.
  // Otherwise, the value of the node->cas_addr is stored in %%rax.
  genln("  lock cmpxchg %s, (%%rdi)", reg_dx(size));
  genln("  sete %%cl");
  genln("  je 1f");
  genln("  mov %s, (%%r8)", reg_ax(size));
  genln("1:");
  genln("  movzx %%cl, %%eax");
}

static void gen_num(Node* node) {
  switch (node->type->kind) {
    case TY_FLOAT: {
      union {
        float f32;
        uint32_t u32;
      } val = {node->float_val};
      genln("  mov $%u, %%eax # float %Lf", val.u32, node->float_val);
      genln("  movq %%rax, %%xmm0");
      return;
    }
    case TY_DOUBLE: {
      union {
        double f64;
        uint64_t u64;
      } val = {node->float_val};
      genln("  mov $%lu, %%rax # double %Lf", val.u64, node->float_val);
      genln("  movq %%rax, %%xmm0");
      return;
    }
    case TY_LDOUBLE: {
      union {
        long double f80;
        uint64_t u64[2];
      } val;
      memset(&val, 0, sizeof(val));
      val.f80 = node->float_val;
      genln("  mov $%lu, %%rax  # double %Lf", val.u64[0], node->float_val);
      genln("  mov %%rax, -16(%%rsp)");
      genln("  mov $%lu, %%rax", val.u64[1]);
      genln("  mov %%rax, -8(%%rsp)");
      genln("  fldt -16(%%rsp)");
      return;
    }
    default:
      genln("  mov $%ld, %%rax", node->int_val);
      return;
  }
}

static void gen_logor(Node* node) {
  int label = count_label();

  gen_expr(node->lhs);
  cmp_zero(node->lhs->type);

  genln("  jne .Ltrue%d", label);

  gen_expr(node->rhs);
  cmp_zero(node->rhs->type);

  genln("  jne .Ltrue%d", label);

  genln("  mov $0, %%rax");

  genln("  jmp .Lend%d", label);

  genln(".Ltrue%d:", label);
  genln("  mov $1, %%rax");

  genln(".Lend%d:", label);
}

static void gen_logand(Node* node) {
  int label = count_label();

  gen_expr(node->lhs);
  cmp_zero(node->lhs->type);

  genln("  je .Lfalse%d", label);

  gen_expr(node->rhs);
  cmp_zero(node->rhs->type);

  genln("  je .Lfalse%d", label);

  genln("  mov $1, %%rax");

  genln("  jmp .Lend%d", label);

  genln(".Lfalse%d:", label);
  genln("  mov $0, %%rax");

  genln(".Lend%d:", label);
}

static void gen_float_bin_expr(Node* node) {
  switch (node->lhs->type->kind) {
    case TY_FLOAT:
    case TY_DOUBLE: {
      gen_expr(node->rhs);
      pushf("%xmm0");
      gen_expr(node->lhs);
      popf("%xmm1");

      char* size = node->lhs->type->kind == TY_FLOAT ? "ss" : "sd";

      switch (node->kind) {
        case ND_EQ:
          genln("  ucomi%s %%xmm0, %%xmm1", size);
          genln("  sete %%al");
          genln("  setnp %%dl");
          genln("  and %%dl, %%al");
          genln("  and $1, %%al");
          genln("  movzx %%al, %%rax");
          return;
        case ND_NE:
          genln("  ucomi%s %%xmm0, %%xmm1", size);
          genln("  setne %%al");
          genln("  setp %%dl");
          genln("  or %%dl, %%al");
          genln("  and $1, %%al");
          genln("  movzx %%al, %%rax");
          return;
        case ND_LT:
          genln("  ucomi%s %%xmm0, %%xmm1", size);
          genln("  seta %%al");
          genln("  and $1, %%al");
          genln("  movzx %%al, %%rax");
          return;
        case ND_LE:
          genln("  ucomi%s %%xmm0, %%xmm1", size);
          genln("  setae %%al");
          genln("  and $1, %%al");
          genln("  movzx %%al, %%rax");
          return;
        case ND_ADD:
          genln("  add%s %%xmm1, %%xmm0", size);
          return;
        case ND_SUB:
          genln("  sub%s %%xmm1, %%xmm0", size);
          return;
        case ND_MUL:
          genln("  mul%s %%xmm1, %%xmm0", size);
          return;
        case ND_DIV:
          genln("  div%s %%xmm1, %%xmm0", size);
          return;
        default:
          error_token(node->token, "invalid expression");
          return;
      }
    }
    case TY_LDOUBLE:
      gen_expr(node->lhs);
      gen_expr(node->rhs);

      switch (node->kind) {
        case ND_EQ:
          genln("  fcomip");
          genln("  fstp %%st(0)");
          genln("  sete %%al");
          genln("  movzx %%al, %%rax");
          return;
        case ND_NE:
          genln("  fcomip");
          genln("  fstp %%st(0)");
          genln("  setne %%al");
          genln("  movzx %%al, %%rax");
          return;
        case ND_LT:
          genln("  fcomip");
          genln("  fstp %%st(0)");
          genln("  seta %%al");
          genln("  movzx %%al, %%rax");
          return;
        case ND_LE:
          genln("  fcomip");
          genln("  fstp %%st(0)");
          genln("  setae %%al");
          genln("  movzx %%al, %%rax");
          return;
        case ND_ADD:
          genln("  faddp");
          return;
        case ND_SUB:
          genln("  fsubrp");
          return;
        case ND_MUL:
          genln("  fmulp");
          return;
        case ND_DIV:
          genln("  fdivrp");
          return;
        default:
          error_token(node->token, "invalid expression");
          return;
      }
    default:
      UNREACHABLE("expected a float but got %d\n", node->lhs->type->kind);
  }
}

static void gen_bin_expr(Node* node) {
  if (is_float_type(node->lhs->type)) {
    gen_float_bin_expr(node);
    return;
  }

  gen_expr(node->lhs);
  push("%rax");
  gen_expr(node->rhs);
  genln("  mov %%rax, %%rdi");
  pop("%rax");

  char* ax;
  char* di;
  char* dx;
  if (node->lhs->type->size <= 4 && node->rhs->type->size <= 4) {
    ax = "%eax";
    di = "%edi";
    dx = "%edx";
  } else {
    ax = "%rax";
    di = "%rdi";
    dx = "%rdx";
  }

  switch (node->kind) {
    case ND_BITOR:
      genln("  or %s, %s", di, ax);
      return;
    case ND_BITXOR:
      genln("  xor %s, %s", di, ax);
      return;
    case ND_BITAND:
      genln("  and %s, %s", di, ax);
      return;
    case ND_EQ:
      genln("  cmp %s, %s", di, ax);
      genln("  sete %%al");
      genln("  movzb %%al, %%rax");
      return;
    case ND_NE:
      genln("  cmp %s, %s", di, ax);
      genln("  setne %%al");
      genln("  movzb %%al, %%rax");
      return;
    case ND_LT:
      genln("  cmp %s, %s", di, ax);
      if (node->lhs->type->is_unsigned) {
        genln("  setb %%al");
      } else {
        genln("  setl %%al");
      }
      genln("  movzb %%al, %%rax");
      return;
    case ND_LE:
      genln("  cmp %s, %s", di, ax);
      if (node->lhs->type->is_unsigned) {
        genln("  setbe %%al");
      } else {
        genln("  setle %%al");
      }
      genln("  movzb %%al, %%rax");
      return;
    case ND_LSHIFT:
      genln("  mov %%rdi, %%rcx");
      genln("  shl %%cl, %s", ax);
      return;
    case ND_RSHIFT:
      genln("  mov %%rdi, %%rcx");
      if (node->lhs->type->is_unsigned) {
        genln("  shr %%cl, %s", ax);
      } else {
        genln("  sar %%cl, %s", ax);
      }
      return;
    case ND_ADD:
      genln("  add %s, %s", di, ax);
      return;
    case ND_SUB:
      genln("  sub %s, %s", di, ax);
      return;
    case ND_MUL:
      genln("  imul %s, %s", di, ax);
      return;
    case ND_DIV:
      if (node->type->is_unsigned) {
        genln("  mov $0, %s", dx);
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
        genln("  mov $0, %s", dx);
        genln("  div %s", di);
      } else {
        if (node->rhs->type->size == 8) {
          genln("  cqo");
        } else {
          genln("  cdq");
        }
        genln("  idiv %s", di);
      }

      genln("  mov %%rdx, %%rax");
      return;
    default:
      UNREACHABLE("expected an expression but got %d", node->kind);
  }
}

static void gen_expr(Node* node) {
  genln("  .loc %d %d", node->token->file->index, node->token->line);

  switch (node->kind) {
    case ND_ASSIGN:
      gen_assign(node);
      return;
    case ND_COMMA:
      gen_expr(node->lhs);
      gen_expr(node->rhs);
      return;
    case ND_COND:
      gen_cond(node);
      return;
    case ND_CAST:
      gen_expr(node->lhs);
      cast(node->type, node->lhs->type);
      return;
    case ND_NEG:
      gen_neg(node);
      return;
    case ND_ADDR:
      gen_addr(node->lhs);
      return;
    case ND_LABEL_ADDR:
      genln("  lea %s(%%rip), %%rax", node->label_id);
      return;
    case ND_DEREF:
      gen_expr(node->lhs);
      load(node);
      return;
    case ND_NOT:
      gen_expr(node->lhs);
      cmp_zero(node->lhs->type);
      genln("  sete %%al");
      genln("  movzx %%al, %%rax");
      return;
    case ND_BITNOT:
      gen_expr(node->lhs);
      genln("  not %%rax");
      return;
    case ND_STMT_EXPR:
      for (Node* stmt = node->body; stmt; stmt = stmt->next) {
        gen_stmt(stmt);
      }
      return;
    case ND_FUNC:
    case ND_GVAR:
    case ND_LVAR:
      gen_addr(node);
      load(node);
      return;
    case ND_MEMBER:
      gen_addr(node);
      load(node);

      Member* mem = node->mem;
      if (mem->is_bitfield) {
        genln("  shl $%d, %%rax", 64 - mem->bit_offset - mem->bit_width);
        if (mem->type->is_unsigned) {
          genln("  shr $%d, %%rax", 64 - mem->bit_width);
        } else {
          genln("  sar $%d, %%rax", 64 - mem->bit_width);
        }
      }
      return;
    case ND_FUNCCALL:
      gen_funccall(node);
      return;
    case ND_ATOMIC_CAS:
      gen_atomic_cas(node);
      return;
    case ND_ATOMIC_EXCH:
      gen_expr(node->lhs);
      push("%rax");
      gen_expr(node->rhs);
      pop("%rdi");

      genln("  xchg %s, (%%rdi)", reg_ax(node->lhs->type->size));
      return;
    case ND_NUM:
      gen_num(node);
      return;
    case ND_NULL:
      return;
    case ND_MEMZERO:
      genln("  mov $%d, %%rcx", node->type->size);
      genln("  lea %d(%%rbp), %%rdi", -1 * node->obj->offset);
      genln("  mov $0, %%al");
      genln("  rep stosb");
      return;
    case ND_LOGOR:
      gen_logor(node);
      return;
    case ND_LOGAND:
      gen_logand(node);
      return;
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
      UNREACHABLE("expected an expression but got %d", node->kind);
  }

  gen_bin_expr(node);
}

static void gen_if(Node* node) {
  int label = count_label();

  gen_expr(node->cond);
  cmp_zero(node->cond->type);

  genln("  je .Lelse%d", label);

  gen_stmt(node->then);

  genln("  jmp .Lend%d", label);

  genln(".Lelse%d:", label);
  if (node->els) {
    gen_stmt(node->els);
  }

  genln(".Lend%d:", label);
}

static void gen_switch(Node* node) {
  gen_expr(node->cond);

  char* ax = node->cond->type->size == 8 ? "%rax" : "%eax";
  char* di = node->cond->type->size == 8 ? "%rdi" : "%edi";
  for (Node* c = node->cases; c; c = c->cases) {
    if (c->case_begin == c->case_end) {
      genln("  cmp $%ld, %s", c->case_begin, ax);
      genln("  je %s", c->label_id);
      continue;
    }

    genln("  mov %s, %s", ax, di);
    genln("  sub $%ld, %s", c->case_begin, di);
    genln("  cmp $%ld, %s", c->case_end - c->case_begin, ax);
    genln("  jbe %s", c->label_id);
  }

  if (node->default_label_id) {
    genln("  jmp %s", node->default_label_id);
  }
  genln("  jmp %s", node->break_label_id);

  gen_stmt(node->then);

  genln("%s:", node->break_label_id);
}

static void gen_for(Node* node) {
  int label = count_label();

  if (node->init) {
    gen_stmt(node->init);
  }

  genln(".Lbegin%d:", label);
  if (node->cond) {
    gen_expr(node->cond);
    cmp_zero(node->cond->type);

    genln("  je %s", node->break_label_id);
  }

  gen_stmt(node->then);

  genln("%s:", node->continue_label_id);

  if (node->inc) {
    gen_expr(node->inc);
  }

  genln("  jmp .Lbegin%d", label);

  genln("%s:", node->break_label_id);
}

static void gen_do(Node* node) {
  int label = count_label();

  genln(".Lbegin%d:", label);

  gen_stmt(node->then);

  genln("%s:", node->continue_label_id);

  gen_expr(node->cond);
  cmp_zero(node->cond->type);
  genln("  jne .Lbegin%d", label);

  genln("%s:", node->break_label_id);
}

static void return_composite_val_via_ptr(Obj* func) {
  genln("  mov %d(%%rbp), %%rdi", -1 * func->ptr_to_return_val->obj->offset);

  for (int i = 0; i < func->type->size; i++) {
    genln("  mov %d(%%rax), %%dl", i);
    genln("  mov %%dl, %d(%%rdi)", i);
  }
}

static void return_composite_val_via_regs(Node* node) {
  int general_cnt = 0;
  int float_cnt = 0;

  genln("  mov %%rax, %%rdi");

  int size = MIN(node->type->size, 8);
  if (have_float_data(node->type, 0, 8)) {
    switch (size) {
      case 4:
        genln("  movss (%%rdi), %%xmm0");
        break;
      case 8:
        genln("  movsd (%%rdi), %%xmm0");
        break;
    }
    float_cnt++;
  } else {
    genln("  mov $0, %%rax");
    for (int i = size - 1; i >= 0; i--) {
      genln("  shl $8, %%rax");
      genln("  mov %d(%%rdi), %%al", i);
    }
    general_cnt++;
  }

  if (node->type->size > 8) {
    size = node->type->size - 8;
    if (have_float_data(node->type, 8, 16)) {
      switch (size) {
        case 4:
          genln("  movss 8(%%rdi), %%xmm%d", float_cnt);
          break;
        case 8:
          genln("  movsd 8(%%rdi), %%xmm%d", float_cnt);
          break;
      }
    } else {
      char* reg8 = general_cnt == 0 ? "%al" : "%dl";
      char* reg64 = general_cnt == 0 ? "%rax" : "%rdx";
      genln("  mov $0, %s", reg64);
      for (int i = size - 1; i >= 0; i--) {
        genln("  shl $8, %s", reg64);
        genln("  mov %d(%%rdi), %s", 8 + i, reg8);
      }
    }
  }
}

static void gen_return(Node* node) {
  if (node->lhs) {
    gen_expr(node->lhs);

    if (is_composite_type(node->lhs->type)) {
      if (node->lhs->type->size > 16) {
        return_composite_val_via_ptr(current_func);
      } else {
        return_composite_val_via_regs(node->lhs);
      }
    }
  }

  genln("  jmp .Lreturn%d", func_cnt);
}

static void gen_stmt(Node* node) {
  genln("  .loc %d %d", node->token->file->index, node->token->line);

  switch (node->kind) {
    case ND_BLOCK:
      for (Node* body = node->body; body; body = body->next) {
        gen_stmt(body);
      }
      return;
    case ND_IF:
      gen_if(node);
      return;
    case ND_SWITCH:
      gen_switch(node);
      return;
    case ND_CASE:
      genln("%s:", node->label_id);
      gen_stmt(node->lhs);
      return;
    case ND_FOR:
      gen_for(node);
      return;
    case ND_DO:
      gen_do(node);
      return;
    case ND_RETURN:
      gen_return(node);
      return;
    case ND_LABEL:
      genln("%s:", node->label_id);
      gen_stmt(node->lhs);
      return;
    case ND_GOTO:
      if (node->lhs) {
        gen_expr(node->lhs);
        genln("  jmp *%%rax");
        return;
      }

      genln("  jmp %s", node->label_id);
      return;
    case ND_ASM:
      genln("  %s", node->asm_str);
      return;
    case ND_EXPR_STMT:
      gen_expr(node->lhs);
      return;
    default:
      gen_expr(node);
  }
}

static void adjust_var_alignment(Obj* var) {
  // AMD64 System V ABI requires an array whose size is at least 16 bytes to have at least 16 bytes alignment.
  if (var->type->kind == TY_ARRAY && var->type->size >= 16) {
    var->type->alignment = MAX(var->type->alignment, 16);
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

    adjust_var_alignment(var->obj);

    if (var->obj->is_static) {
      genln(".local %s", var->obj->name);
    } else {
      genln(".global %s", var->obj->name);
    }

    if (common_symbols_enabled && var->obj->is_tentative) {
      genln(".comm %s, %d, %d", var->obj->name, var->obj->type->size, var->obj->type->alignment);
      continue;
    }

    if (var->obj->val) {
      if (var->obj->is_thread_local) {
        genln(".section .tdata, \"awT\", @progbits");
      } else {
        genln(".data");
      }
      genln(".type %s, @object", var->obj->name);
      genln(".size %s, %d", var->obj->name, var->obj->type->size);
      genln(".align %d", var->obj->type->alignment);
      genln("%s:", var->obj->name);

      Relocation* reloc = var->obj->relocs;
      int offset = 0;
      while (offset < var->obj->type->size) {
        if (reloc && offset == reloc->offset) {
          genln("  .quad %s%+ld", *reloc->label, reloc->addend);
          reloc = reloc->next;
          offset += 8;
          continue;
        }

        genln("  .byte %d", var->obj->val[offset++]);
      }

      continue;
    }

    if (var->obj->is_thread_local) {
      genln(".section .tbss, \"awT\", @nobits");
    } else {
      genln(".bss");
    }
    genln(".align %d", var->obj->type->alignment);
    genln("%s:", var->obj->name);
    genln("  .zero %d", var->obj->type->size);
  }
}

static void store_va_args(Obj* func) {
  int general_cnt = 0;
  int float_cnt = 0;
  for (Node* param = func->params; param; param = param->next) {
    if (is_float_type(param->type)) {
      float_cnt++;
    } else {
      general_cnt++;
    }
  }

  int offset = func->va_area->offset;

  // to assign __va_area__ to __va_elem
  // set __va_area__ as __va_elem manually in memory
  // __va_elem.gp_offset (unsigned int)
  genln("  movl $%d, %d(%%rbp)", general_cnt * 8, -1 * offset);
  // __va_elem.fp_offset (unsigned int)
  genln("  movl $%d, %d(%%rbp)", float_cnt * 8 + 48, -1 * (offset - 4));
  // __va_elem.overflow_arg_area (void*)
  genln("  movq %%rbp, %d(%%rbp)", -1 * (offset - 8));
  genln("  addq $16, %d(%%rbp)", -1 * (offset - 8));
  // __va_elem.reg_save_area (void*)
  genln("  movq %%rbp, %d(%%rbp)", -1 * (offset - 16));
  genln("  subq $%d, %d(%%rbp)", offset - 24, -1 * (offset - 16));
  genln("  movq %%rdi, %d(%%rbp)", -1 * (offset - 24));
  genln("  movq %%rsi, %d(%%rbp)", -1 * (offset - 32));
  genln("  movq %%rdx, %d(%%rbp)", -1 * (offset - 40));
  genln("  movq %%rcx, %d(%%rbp)", -1 * (offset - 48));
  genln("  movq %%r8, %d(%%rbp)", -1 * (offset - 56));
  genln("  movq %%r9, %d(%%rbp)", -1 * (offset - 64));
  genln("  movsd %%xmm0, %d(%%rbp)", -1 * (offset - 72));
  genln("  movsd %%xmm1, %d(%%rbp)", -1 * (offset - 80));
  genln("  movsd %%xmm2, %d(%%rbp)", -1 * (offset - 88));
  genln("  movsd %%xmm3, %d(%%rbp)", -1 * (offset - 96));
  genln("  movsd %%xmm4, %d(%%rbp)", -1 * (offset - 104));
  genln("  movsd %%xmm5, %d(%%rbp)", -1 * (offset - 112));
  genln("  movsd %%xmm6, %d(%%rbp)", -1 * (offset - 120));
  genln("  movsd %%xmm7, %d(%%rbp)", -1 * (offset - 128));
}

static void assign_param_offsets_for_passed_by_stack_args(Obj* func) {
  int general_cnt = 0;
  int float_cnt = 0;

  // The passed-by-stack arguments lives in the stack of the caller function,
  // and there are 16 bytes offset to the location from the stack of this function
  // because of the return address and the base pointer.
  int offset = 16;

  for (Node* param = func->params; param; param = param->next) {
    switch (param->type->kind) {
      case TY_STRUCT:
      case TY_UNION: {
        if (param->type->size > 16) {
          break;
        }

        bool former_float = have_float_data(param->type, 0, 8);
        bool latter_float = have_float_data(param->type, 8, 16);
        if (general_cnt + !former_float + !latter_float > MAX_GENERAL_REG_ARGS
            || float_cnt + former_float + latter_float > MAX_FLOAT_REG_ARGS) {
          break;
        }

        general_cnt += !former_float + !latter_float;
        float_cnt += former_float + latter_float;
        continue;
      }
      case TY_FLOAT:
      case TY_DOUBLE:
        if (++float_cnt <= MAX_FLOAT_REG_ARGS) {
          continue;
        }
        break;
      case TY_LDOUBLE:
        break;
      default:
        if (++general_cnt <= MAX_GENERAL_REG_ARGS) {
          continue;
        }
        break;
    }

    offset = align_up(offset, 8);
    param->obj->offset = -offset;
    offset += param->type->size;
  }
}

static int assign_lvar_offset(Obj* var) {
  if (!var) {
    return 0;
  }

  int last_offset = assign_lvar_offset(var->next);

  adjust_var_alignment(var);

  // Local variables which store passed-by-stack arguments do not live
  // in the stack of this function but that of the caller function.
  //
  // Static local variables do not live in the stack of this function
  // as they are in the place where global variables lives in fact.
  if (var->offset < 0 || var->is_static) {
    return last_offset;
  }

  var->offset = align_up(last_offset + var->type->size, var->type->alignment);

  return var->offset;
}

static void assign_lvar_offsets(Obj* func) {
  assign_param_offsets_for_passed_by_stack_args(func);

  int last_offset = assign_lvar_offset(func->lvars);

  func->stack_size = align_up(last_offset, 16);
}

static void store_general_arg(int size, int offset, int n) {
  switch (size) {
    case 1:
      genln("  mov %s, %d(%%rax)", arg_regs8[n], offset);
      break;
    case 2:
      genln("  mov %s, %d(%%rax)", arg_regs16[n], offset);
      break;
    case 4:
      genln("  mov %s, %d(%%rax)", arg_regs32[n], offset);
      break;
    case 8:
      genln("  mov %s, %d(%%rax)", arg_regs64[n], offset);
      break;
    default:
      for (int i = 0; i < size; i++) {
        genln("  mov %s, %d(%%rax)", arg_regs8[n], offset + i);
        genln("  shr $8, %s", arg_regs64[n]);
      }
  }
}

static void store_float_arg(int size, int offset, int n) {
  switch (size) {
    case 4:
      genln("  movss %%xmm%d, %d(%%rax)", n, offset);
      break;
    case 8:
      genln("  movsd %%xmm%d, %d(%%rax)", n, offset);
      break;
  }
}

static void store_args(Obj* func) {
  int general_cnt = 0;
  int float_cnt = 0;

  if (func->ptr_to_return_val) {
    gen_addr(func->ptr_to_return_val);
    store_general_arg(func->ptr_to_return_val->type->size, 0, general_cnt++);
  }

  for (Node* param = func->params; param; param = param->next) {
    if (param->obj->offset < 0) {
      continue;
    }

    gen_addr(param);

    switch (param->type->kind) {
      case TY_STRUCT:
      case TY_UNION: {
        int size = MIN(param->type->size, 8);
        if (have_float_data(param->type, 0, 8)) {
          store_float_arg(size, 0, float_cnt++);
        } else {
          store_general_arg(size, 0, general_cnt++);
        }
        if (param->type->size > 8) {
          int size = param->type->size - 8;
          if (have_float_data(param->type, 8, 16)) {
            store_float_arg(size, 8, float_cnt++);
          } else {
            store_general_arg(size, 8, general_cnt++);
          }
        }
        break;
      }
      case TY_FLOAT:
      case TY_DOUBLE:
        store_float_arg(param->type->size, 0, float_cnt++);
        break;
      default:
        store_general_arg(param->type->size, 0, general_cnt++);
        break;
    }
  }
}

static void gen_text(TopLevelObj* codes) {
  for (TopLevelObj* func = codes; func; func = func->next) {
    if (func->obj->kind != OJ_FUNC || !func->obj->is_definition) {
      continue;
    }

    if (!func->obj->is_referred) {
      continue;
    }

    current_func = func->obj;

    assign_lvar_offsets(func->obj);

    if (func->obj->is_static) {
      genln(".local %s", func->obj->name);
    } else {
      genln(".global %s", func->obj->name);
    }

    genln(".text");
    genln(".type %s, @function", func->obj->name);
    genln("%s:", func->obj->name);

    push_outside_frame("%rbp");
    genln("  mov %%rsp, %%rbp");
    genln("  sub $%d, %%rsp", func->obj->stack_size);

    genln("  mov %%rsp, %d(%%rbp)", -1 * func->obj->alloca_bottom->offset);

    if (func->obj->va_area) {
      store_va_args(func->obj);
    }

    store_args(func->obj);

    gen_stmt(func->obj->body);

    // Acco%%rding to C spec, the main function should return 0 though it has no return statement.
    // The retrun statement jumps to the return label below,
    // so this instruction does not override the returne value.
    if (equal_to_str(func->obj->name, "main")) {
      genln("  mov $0, %%rax");
    }

    genln(".Lreturn%d:", func_cnt++);
    genln("  mov %%rbp, %%rsp");
    pop_outside_frame("%rbp");
    genln("  ret");

    current_func = NULL;
  }
}

static void assert_depth_offset(char* name, int depth) {
  if (depth != 0) {
    UNREACHABLE("%s: push and pop do not offset each other: %d", name, depth);
  }
}

void gen(char* output_filename, TopLevelObj* codes) {
  char* buf;
  size_t buf_len = 0;
  output_file = open_memstream(&buf, &buf_len);

  for (File* file = files; file; file = file->next) {
    genln(".file %d \"%s\"", file->index, file->name);
  }

  gen_data(codes);
  gen_text(codes);

  fclose(output_file);

  assert_depth_offset("depth_outside_frame", depth_outside_frame);
  assert_depth_offset("depth", depth);

  FILE* file = open_output_file(output_filename);
  fwrite(buf, buf_len, 1, file);
  fclose(file);
}