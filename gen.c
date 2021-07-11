#include "cc.h"

static FILE* output_file;

static int depth_outside_frame = 0;
static int depth = 0;

static Obj* current_func;
static int func_cnt = 0;

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
static char i32_f80[] = "mov [rsp-4], eax; fild DWORD PTR [rsp-4]";
static char i64_f32[] = "cvtsi2ss xmm0, rax";
static char i64_f64[] = "cvtsi2sd xmm0, rax";
static char i64_f80[] = "movq [rsp-8], rax; fildll [rsp-8]";
static char z8_32[] = "movzx eax, al";
static char z16_32[] = "movzx eax, ax";
static char z32_64[] = "mov eax, eax";
static char u32_f32[] = "mov eax, eax; cvtsi2ss xmm0, rax";
static char u32_f64[] = "mov eax, eax; cvtsi2sd xmm0, rax";
static char u32_f80[] = "mov eax, eax; mov [rsp-8], rax; fildll [rsp-8]";
static char u64_f32[] = "cvtsi2ss xmm0, rax";
static char u64_f64[]
  = "test rax, rax; js 1f; pxor xmm0, xmm0; cvtsi2sd xmm0, rax; jmp 2f; "
    "1: mov rdi, rax; and eax, 1; pxor xmm0, xmm0; shr rdi; "
    "or rdi, rax; cvtsi2sd xmm0, rdi; addsd xmm0, xmm0; 2:";
static char u64_f80[]
  = "mov [rsp-8], rax; fildq [rsp-8]; test rax, rax; jns 1f;"
    "mov eax, 1602224128; mov [rsp-4], eax; fadd DWORD PTR [rsp-4]; 1:";
static char f32_i8[] = "cvttss2si eax, xmm0; movsx eax, al";
static char f32_i16[] = "cvttss2si eax, xmm0; movsx eax, ax";
static char f32_i32[] = "cvttss2si eax, xmm0";
static char f32_i64[] = "cvttss2si rax, xmm0";
static char f32_u8[] = "cvttss2si eax, xmm0; movzx eax, al";
static char f32_u16[] = "cvttss2si eax, xmm0; movzx eax, ax";
static char f32_u32[] = "cvttss2si rax, xmm0";
static char f32_u64[] = "cvttss2si rax, xmm0";
static char f32_f64[] = "cvtss2sd xmm0, xmm0";
static char f32_f80[] = "movss [rsp-4], xmm0; fld DWORD PTR [rsp-4]";
static char f64_i8[] = "cvttsd2si eax, xmm0; movsx eax, al";
static char f64_i16[] = "cvttsd2si eax, xmm0; movsx eax, ax";
static char f64_i32[] = "cvttsd2si eax, xmm0";
static char f64_i64[] = "cvttsd2si rax, xmm0";
static char f64_u8[] = "cvttsd2si eax, xmm0; movzx eax, al";
static char f64_u16[] = "cvttsd2si eax, xmm0; movzx eax, ax";
static char f64_u32[] = "cvttsd2si rax, xmm0";
static char f64_u64[] = "cvttsd2si rax, xmm0";
static char f64_f32[] = "cvtsd2ss xmm0, xmm0";
static char f64_f80[] = "movsd [rsp-8], xmm0; fld QWORD PTR [rsp-8]";

#define FROM_F80_1 "fnstcw [rsp-10]; movzx eax, WORD PTR [rsp-10]; or ah, 12; mov [rsp-12], ax; fldcw [rsp-12];"
#define FROM_F80_2 " [rsp-24]; fldcw [rsp-10]; "

static char f80_i8[] = FROM_F80_1 "fistp WORD PTR" FROM_F80_2 "movsx eax, BYTE PTR [rsp-24]";
static char f80_i16[] = FROM_F80_1 "fistp WORD PTR" FROM_F80_2 "movzx eax, BYTE PTR [rsp-24]";
static char f80_i32[] = FROM_F80_1 "fistp DWORD PTR" FROM_F80_2 "mov eax, [rsp-24]";
static char f80_i64[] = FROM_F80_1 "fistp QWORD PTR" FROM_F80_2 "mov rax, [rsp-24]";
static char f80_u8[] = FROM_F80_1 "fistp WORD PTR" FROM_F80_2 "movzx eax, BYTE PTR [rsp-24]";
static char f80_u16[] = FROM_F80_1 "fistp DWORD PTR" FROM_F80_2 "movzx eax, WORD PTR [rsp-24]";
static char f80_u32[] = FROM_F80_1 "fistp DWORD PTR" FROM_F80_2 "mov eax, [rsp-24]";
static char f80_u64[] = FROM_F80_1 "fistp QWORD PTR" FROM_F80_2 "mov rax, [rsp-24]";
static char f80_f32[] = "fstp DWORD PTR [rsp-8]; movss xmm0, [rsp-8]";
static char f80_f64[] = "fstp QWORD PTR [rsp-8]; movsd xmm0, [rsp-8]";
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
    case ND_COMMA:
      gen_expr(node->lhs);
      gen_addr(node->rhs);
      break;
    case ND_DEREF:
      gen_expr(node->lhs);
      break;
    case ND_FUNC:
      if (as_pic) {
        genln("  mov rax, %s@GOTPCREL[rip]", node->obj->name);
        break;
      }

      if (node->obj->is_definition) {
        genln("  lea rax, %s[rip]", node->obj->name);
      } else {
        genln("  mov rax, %s@GOTPCREL[rip]", node->obj->name);
      }
      break;
    case ND_GVAR:
      if (as_pic) {
        if (node->obj->is_thread_local) {
          genln("  data16 lea rdi, %s@TLSGD[rip]", node->obj->name);
          genln("  .value 0x6666");
          genln("  rex64");
          genln("  call __tls_get_addr@PLT");
          break;
        }

        genln("  mov rax, %s@GOTPCREL[rip]", node->obj->name);
        break;
      }

      if (node->obj->is_thread_local) {
        genln("  lea rax, %s@TPOFF", node->obj->name);
        genln("  add rax, fs:0");
        break;
      }
      genln("  lea rax, %s[rip]", node->obj->name);
      break;
    case ND_LVAR:
      // The address of VLA is stored in this variable unlike array, so load it.
      if (node->type->kind == TY_VL_ARRAY) {
        genln("  mov rax, [rbp-%d]", node->obj->offset);
      } else {
        genln("  lea rax, [rbp-%d]", node->obj->offset);
      }
      break;
    case ND_MEMBER:
      gen_addr(node->lhs);
      genln("  add rax, %d", node->mem->offset);
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
  if (!is_composite_type(type)) {
    UNREACHABLE("expected a composite typed value but got %d", type->kind);
  }

  int size = align_up(type->size, 8);
  genln("  sub rsp, %d", size);
  depth += size / 8;

  for (int i = 0; i < type->size; i++) {
    genln("  mov r10b, [%s+%d]", reg, i);
    genln("  mov [rsp+%d], r10b", i);
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
      genln("  xorps xmm1, xmm1");
      genln("  ucomiss xmm0, xmm1");
      return;
    case TY_DOUBLE:
      genln("  xorpd xmm1, xmm1");
      genln("  ucomisd xmm0, xmm1");
      return;
    case TY_LDOUBLE:
      genln("  fldz");
      genln("  fucomip");
      genln("  fstp st(0)");
      return;
    default:
      if (is_int_type(type) && type->size <= 4) {
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
    case TY_VL_ARRAY:
      return;
    case TY_FLOAT:
      genln("  movss xmm0, [rax]");
      return;
    case TY_DOUBLE:
      genln("  movsd xmm0, [rax]");
      return;
    case TY_LDOUBLE:
      genln("  fldt [rax]");
      return;
    default: {
      char* ins = node->type->is_unsigned ? "movz" : "movs";
      switch (node->type->size) {
        case 1:
          genln("  %sx eax, BYTE PTR [rax]", ins);
          return;
        case 2:
          genln("  %sx eax, WORD PTR [rax]", ins);
          return;
        case 4:
          genln("  movsx rax, DWORD PTR [rax]");
          return;
        default:
          genln("  mov rax, [rax]");
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
    case TY_LDOUBLE:
      genln("  fstpt [rdi]");
      return;
    default:
      switch (node->type->size) {
        case 1:
          genln("  mov [rdi], al");
          return;
        case 2:
          genln("  mov [rdi], ax");
          return;
        case 4:
          genln("  mov [rdi], eax");
          return;
        default:
          genln("  mov [rdi], rax");
          return;
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

static void gen_assign(Node* node) {
  gen_addr(node->lhs);
  push("rax");
  gen_expr(node->rhs);

  if (node->lhs->kind != ND_MEMBER || !node->lhs->mem->is_bitfield) {
    pop("rdi");
    store(node);
    return;
  }

  // When the assignee (node->lhs) is the member of a compsite type and bitfield,
  // calculate the value which should be the result of this assignment in the node->rhs
  // so that this assign expression can do the assignment of a value of the composite type as normally.
  Member* mem = node->lhs->mem;
  genln("  mov r8, rax");  // Keep the node->rhs value as the last expression

  // The bitfield value of the member
  genln("  mov rdi, rax");
  genln("  and rdi, %ld", (1L << mem->bit_width) - 1);
  genln("  shl rdi, %d", mem->bit_offset);

  // Get the value of the node->lhs without popping stack.
  genln("  mov rax, [rsp]");
  load(node->lhs);
  // Merge the node->lhs value of the composite type with the bitfield value.
  int mask = ((1L << mem->bit_width) - 1) << mem->bit_offset;
  genln("  mov r9, %d", ~mask);
  genln("  and rax, r9");
  genln("  or rax, rdi");

  pop("rdi");
  store(node);

  genln("  mov rax, r8");
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
      return;
    case TY_LDOUBLE:
      genln("  fchs");
      return;
    default:
      genln("  neg rax");
      return;
  }
}

static void store_returned_by_reg_val(Node* node) {
  int int_cnt = 0;
  int float_cnt = 0;

  int size = MIN(node->type->size, 8);
  if (have_float_data(node->type, 0, 8)) {
    switch (size) {
      case 4:
        genln("  movss [rbp-%d], xmm0", node->obj->offset);
        break;
      case 8:
        genln("  movsd [rbp-%d], xmm0", node->obj->offset);
        break;
    }
    float_cnt++;
  } else {
    for (int i = 0; i < size; i++) {
      genln("  mov [rbp-%d], al", node->obj->offset - i);
      genln("  shr rax, 8");
    }
    int_cnt++;
  }

  if (node->type->size > 8) {
    size = node->type->size - 8;
    if (have_float_data(node->type, 8, 16)) {
      switch (size) {
        case 4:
          genln("  movss [rbp-%d], xmm%d", node->obj->offset - 8, float_cnt);
          return;
        case 8:
          genln("  movsd [rbp-%d], xmm%d", node->obj->offset - 8, float_cnt);
          return;
      }
    } else {
      char* reg1 = int_cnt == 0 ? "al" : "dl";
      char* reg2 = int_cnt == 0 ? "rax" : "rdx";
      for (int i = 0; i < size; i++) {
        genln("  mov [rbp-%d], %s", node->obj->offset - 8 - i, reg1);
        genln("  shr %s, 8", reg2);
      }
    }
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
    case TY_LDOUBLE:
      genln("  sub rsp, 16");
      genln("  fstpt [rsp]");
      depth += 2;
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
  int int_cnt = 0;
  int float_cnt = 0;

  // When the return values of function calls are struct or union and their size are larger than
  // 16 bytes, the pointers to the values are passed as if they are the first arguments
  // for the callee to fill the return values to the pointers.
  if (node->return_val && node->type->size > 16) {
    int_cnt++;
  }

  for (Node* arg = node->args; arg; arg = arg->next) {
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
          stacked += align_up(arg->type->size, 8) / 8;
          continue;
        }

        bool former_float = have_float_data(arg->type, 0, 8);
        bool latter_float = have_float_data(arg->type, 8, 16);
        if (int_cnt + !former_float + !latter_float > MAX_INT_REG_ARGS
            || float_cnt + former_float + latter_float > MAX_FLOAT_REG_ARGS) {
          arg->is_passed_by_stack = true;
          stacked += align_up(arg->type->size, 8) / 8;
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
      case TY_LDOUBLE:
        arg->is_passed_by_stack = true;
        stacked += 2;
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
  push_passed_by_stack_args(node->args);
  push_passed_by_reg_args(node->args);
  if (node->return_val && node->type->size > 16) {
    gen_addr(node->return_val);
    push("rax");
  }

  return stacked;
}

static bool equal_to_func(Node* node, char* func) {
  return node->kind == ND_FUNC && equal_to_str(node->obj->name, func);
}

static void gen_alloca(Node* node) {
  gen_expr(node->args);
  genln("  mov rdi, rax");

  // Align the arg (the size to allocate) to 16 byte boundary.
  genln("  add rdi, 15");
  genln("  and edi, 0xfffffff0");

  // Copy values pushed on stack at this time and keep having them at the top of stack
  // so that those values can be poped, and leave space for the size to allocate.
  genln("  mov rcx, [rbp-%d]", current_func->alloca_bottom->offset);
  genln("  sub rcx, rsp");
  genln("  mov rax, rsp");
  genln("  sub rsp, rdi");
  genln("  mov rdx, rsp");
  genln("1:");
  genln("  cmp rcx, 0");
  genln("  je  2f");
  genln("  mov r8b, [rax]");
  genln("  mov [rdx], r8b");
  genln("  inc rdx");
  genln("  inc rax");
  genln("  dec rcx");
  genln("  jmp 1b");
  genln("2:");

  // Use the left space for the alloca value and keep it for the next alloca.
  genln("  mov rax, [rbp-%d]", current_func->alloca_bottom->offset);
  genln("  sub rax, rdi");
  genln("  mov [rbp-%d], rax", current_func->alloca_bottom->offset);
}

static void gen_funccall(Node* node) {
  if (equal_to_func(node->lhs, "alloca")) {
    gen_alloca(node);
    return;
  }

  // The number of argments which remain in stack
  // following x8664 psAPI.
  // Those arguments which remain in stack are pushed
  // after aligning the stack pointer to 16 bytes boundary
  // so it is not necessary to align it again on call.
  int stacked = push_args(node);

  // node->lhs may be another function call, which means that
  // it may use arguments registers for its arguments,
  // so resolve the address of function first
  // keeping the values of the arugments of this function call in stack
  // and then pop them, otherwise the poped values of the arguments of this function call
  // will be overridden.
  gen_expr(node->lhs);

  int int_cnt = 0;
  int float_cnt = 0;

  if (node->return_val && node->type->size > 16) {
    pop(arg_regs64[int_cnt++]);
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
      case TY_LDOUBLE:
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

  // The return values of function calls whose types are struct or union and whose size are
  // less than or equal to 16 bytes are returned by up to 2 registers.
  // So store them to the stack of the caller assigned for the return values.
  if (node->return_val && node->type->size <= 16) {
    store_returned_by_reg_val(node->return_val);
    gen_addr(node->return_val);
  }
}

static void gen_num(Node* node) {
  switch (node->type->kind) {
    case TY_FLOAT: {
      union {
        float f32;
        uint32_t u32;
      } val = {node->float_val};
      genln("  mov eax, %u # float %Lf", val.u32, node->float_val);
      genln("  movq xmm0, rax");
      return;
    }
    case TY_DOUBLE: {
      union {
        double f64;
        uint64_t u64;
      } val = {node->float_val};
      genln("  mov rax, %lu # double %Lf", val.u64, node->float_val);
      genln("  movq xmm0, rax");
      return;
    }
    case TY_LDOUBLE: {
      union {
        long double f80;
        uint64_t u64[2];
      } val;
      memset(&val, 0, sizeof(val));
      val.f80 = node->float_val;
      genln("  mov rax, %lu # double %Lf", val.u64[0], node->float_val);
      genln("  mov [rsp-16], rax");
      genln("  mov rax, %lu", val.u64[1]);
      genln("  mov [rsp-8], rax");
      genln("  fldt [rsp-16]");
      return;
    }
    default:
      genln("  mov rax, %ld", node->int_val);
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

  genln("  mov rax, 0");

  genln("  jmp .Lend%d", label);

  genln(".Ltrue%d:", label);
  genln("  mov rax, 1");

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

  genln("  mov rax, 1");

  genln("  jmp .Lend%d", label);

  genln(".Lfalse%d:", label);
  genln("  mov rax, 0");

  genln(".Lend%d:", label);
}

static void gen_float_bin_expr(Node* node) {
  switch (node->lhs->type->kind) {
    case TY_FLOAT:
    case TY_DOUBLE: {
      gen_expr(node->rhs);
      pushf("xmm0");
      gen_expr(node->lhs);
      popf("xmm1");

      char* size = node->lhs->type->kind == TY_FLOAT ? "ss" : "sd";

      switch (node->kind) {
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
    case TY_LDOUBLE:
      gen_expr(node->lhs);
      gen_expr(node->rhs);

      switch (node->kind) {
        case ND_EQ:
          genln("  fcomip");
          genln("  fstp st(0)");
          genln("  sete al");
          genln("  movzx rax, al");
          return;
        case ND_NE:
          genln("  fcomip");
          genln("  fstp st(0)");
          genln("  setne al");
          genln("  movzx rax, al");
          return;
        case ND_LT:
          genln("  fcomip");
          genln("  fstp st(0)");
          genln("  seta al");
          genln("  movzx rax, al");
          return;
        case ND_LE:
          genln("  fcomip");
          genln("  fstp st(0)");
          genln("  setae al");
          genln("  movzx rax, al");
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
      genln("  lea rax, %s[rip]", node->label_id);
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
      gen_addr(node);
      load(node);
      return;
    case ND_MEMBER:
      gen_addr(node);
      load(node);

      Member* mem = node->mem;
      if (mem->is_bitfield) {
        genln("  shl rax, %d", 64 - mem->bit_offset - mem->bit_width);
        if (mem->type->is_unsigned) {
          genln("  shr rax, %d", 64 - mem->bit_width);
        } else {
          genln("  sar rax, %d", 64 - mem->bit_width);
        }
      }
      return;
    case ND_FUNCCALL:
      gen_funccall(node);
      return;
    case ND_NUM:
      gen_num(node);
      return;
    case ND_NULL:
      return;
    case ND_MEMZERO:
      genln("  mov rcx, %d", node->type->size);
      genln("  lea rdi, [rbp-%d]", node->obj->offset);
      genln("  mov al, 0");
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

  char* ax = node->cond->type->size == 8 ? "rax" : "eax";
  char* di = node->cond->type->size == 8 ? "rdi" : "edi";
  for (Node* c = node->cases; c; c = c->cases) {
    if (c->case_begin == c->case_end) {
      genln("  cmp %s, %ld", ax, c->case_begin);
      genln("  je %s", c->label_id);
      continue;
    }

    genln("  mov %s, %s", di, ax);
    genln("  sub %s, %ld", di, c->case_begin);
    genln("  cmp %s, %ld", ax, c->case_end - c->case_begin);
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

  genln("  jmp .Lbegin%d", label);

  genln("%s:", node->break_label_id);
}

static void return_val_via_regs(Node* node) {
  int int_cnt = 0;
  int float_cnt = 0;

  genln("  mov rdi, rax");

  int size = MIN(node->type->size, 8);
  if (have_float_data(node->type, 0, 8)) {
    switch (size) {
      case 4:
        genln("  movss xmm0, [rdi]");
        break;
      case 8:
        genln("  movsd xmm0, [rdi]");
        break;
    }
    float_cnt++;
  } else {
    for (int i = size - 1; i >= 0; i--) {
      genln("  shl rax, 8");
      genln("  mov al, %d[rdi]", i);
    }
    int_cnt++;
  }

  if (node->type->size > 8) {
    size = node->type->size - 8;
    if (have_float_data(node->type, 8, 16)) {
      switch (size) {
        case 4:
          genln("  movss xmm%d, 8[rdi]", float_cnt);
          break;
        case 8:
          genln("  movsd xmm%d, 8[rdi]", float_cnt);
          break;
      }
    } else {
      char* reg1 = int_cnt == 0 ? "al" : "dl";
      char* reg2 = int_cnt == 0 ? "rax" : "rdx";
      for (int i = size - 1; i >= 0; i--) {
        genln("  shl %s, 8", reg2);
        genln("  mov %s, %d[rdi]", reg1, 8 + i);
      }
    }
  }
}

static void gen_return(Node* node) {
  if (node->lhs) {
    gen_expr(node->lhs);

    if (is_composite_type(node->lhs->type)) {
      if (node->lhs->type->size > 16) {
        genln("  mov [rbp-%d], rax", current_func->ptr_to_return_val->obj->offset);
      } else {
        return_val_via_regs(node->lhs);
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
        genln("  jmp rax");
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
      genln(".align %d", var->obj->alignment);
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
    genln(".align %d", var->obj->alignment);
    genln("%s:", var->obj->name);
    genln("  .zero %d", var->obj->type->size);
  }
}

static void store_va_args(Obj* func) {
  int int_cnt = 0;
  int float_cnt = 0;
  for (Node* param = func->params; param; param = param->next) {
    if (is_float_type(param->type)) {
      float_cnt++;
    } else {
      int_cnt++;
    }
  }

  int offset = func->va_area->offset;

  // to assign __va_area__ to __va_elem
  // set __va_area__ as __va_elem manually in memory
  // __va_elem.gp_offset (unsigned int)
  genln("  mov DWORD PTR [rbp-%d], %d", offset, int_cnt * 8);
  // __va_elem.fp_offset (unsigned int)
  genln("  mov DWORD PTR [rbp-%d], %d", offset - 4, float_cnt * 8 + 48);
  // __va_elem.overflow_arg_area (void*)
  genln("  mov QWORD PTR [rbp-%d], rbp", offset - 8);
  genln("  add QWORD PTR [rbp-%d], 16", offset - 8);
  // __va_elem.reg_save_area (void*)
  genln("  mov QWORD PTR [rbp-%d], rbp", offset - 16);
  genln("  sub QWORD PTR [rbp-%d], %d", offset - 16, offset - 24);
  genln("  mov QWORD PTR [rbp-%d], rdi", offset - 24);
  genln("  mov QWORD PTR [rbp-%d], rsi", offset - 32);
  genln("  mov QWORD PTR [rbp-%d], rdx", offset - 40);
  genln("  mov QWORD PTR [rbp-%d], rcx", offset - 48);
  genln("  mov QWORD PTR [rbp-%d], r8", offset - 56);
  genln("  mov QWORD PTR [rbp-%d], r9", offset - 64);
  genln("  movsd [rbp-%d], xmm0", offset - 72);
  genln("  movsd [rbp-%d], xmm1", offset - 80);
  genln("  movsd [rbp-%d], xmm2", offset - 88);
  genln("  movsd [rbp-%d], xmm3", offset - 96);
  genln("  movsd [rbp-%d], xmm4", offset - 104);
  genln("  movsd [rbp-%d], xmm5", offset - 112);
  genln("  movsd [rbp-%d], xmm6", offset - 120);
  genln("  movsd [rbp-%d], xmm7", offset - 128);
}

static void assign_passed_by_stack_arg_offsets(Obj* func) {
  int int_cnt = 0;
  int float_cnt = 0;
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
      case TY_LDOUBLE:
        break;
      default:
        if (++int_cnt <= MAX_INT_REG_ARGS) {
          continue;
        }
        break;
    }

    offset = align_up(offset, 8);
    param->obj->offset = -offset;
    offset += param->type->size;
  }
}

static void store_int_arg(int size, int offset, int n) {
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

static void store_float_arg(int size, int offset, int n) {
  switch (size) {
    case 4:
      genln("  movss [rax+%d], xmm%d", offset, n);
      break;
    case 8:
      genln("  movsd [rax+%d], xmm%d", offset, n);
      break;
  }
}

static void store_args(Obj* func) {
  int int_cnt = 0;
  int float_cnt = 0;

  if (func->ptr_to_return_val) {
    gen_addr(func->ptr_to_return_val);
    store_int_arg(func->ptr_to_return_val->type->size, 0, int_cnt++);
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
          store_int_arg(size, 0, int_cnt++);
        }
        if (param->type->size > 8) {
          int size = param->type->size - 8;
          if (have_float_data(param->type, 8, 16)) {
            store_float_arg(size, 8, float_cnt++);
          } else {
            store_int_arg(size, 8, int_cnt++);
          }
        }
        break;
      }
      case TY_FLOAT:
      case TY_DOUBLE:
        store_float_arg(param->type->size, 0, float_cnt++);
        break;
      default:
        store_int_arg(param->type->size, 0, int_cnt++);
        break;
    }
  }
}

static void gen_text(TopLevelObj* codes) {
  for (TopLevelObj* func = codes; func; func = func->next) {
    if (func->obj->kind != OJ_FUNC || !func->obj->is_definition) {
      continue;
    }

    if (func->obj->is_static && !func->obj->is_referred) {
      continue;
    }

    current_func = func->obj;

    if (func->obj->is_static) {
      genln(".local %s", func->obj->name);
    } else {
      genln(".global %s", func->obj->name);
    }

    genln(".text");
    genln(".type %s, @function", func->obj->name);
    genln("%s:", func->obj->name);

    push_outside_frame("rbp");
    genln("  mov rbp, rsp");
    genln("  sub rsp, %d", func->obj->stack_size);

    genln("  mov [rbp-%d], rsp", func->obj->alloca_bottom->offset);

    if (func->obj->va_area) {
      store_va_args(func->obj);
    }

    assign_passed_by_stack_arg_offsets(func->obj);

    store_args(func->obj);

    gen_stmt(func->obj->body);

    // According to C spec, the main function should return 0 though it has no return statement.
    // The retrun statement jumps to the return label below,
    // so this instruction does not override the returne value.
    if (equal_to_str(func->obj->name, "main")) {
      genln("  mov rax, 0");
    }

    genln(".Lreturn%d:", func_cnt++);
    genln("  mov rsp, rbp");
    pop_outside_frame("rbp");
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

  genln(".intel_syntax noprefix");
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