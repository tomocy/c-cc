#include "cc.h"

static Type* new_type(TypeKind kind, int size, int alignment) {
  Type* type = calloc(1, sizeof(Type));
  type->kind = kind;
  type->size = size;
  type->alignment = alignment;
  return type;
}

Type* new_void_type() {
  return new_type(TY_VOID, 1, 1);
}

Type* new_bool_type() {
  return new_type(TY_BOOL, 1, 1);
}

Type* new_char_type() {
  return new_type(TY_CHAR, 1, 1);
}

Type* new_uchar_type() {
  Type* type = new_char_type();
  type->is_unsigned = true;
  return type;
}

Type* new_short_type() {
  return new_type(TY_SHORT, 2, 2);
}

Type* new_ushort_type() {
  Type* type = new_short_type();
  type->is_unsigned = true;
  return type;
}

Type* new_int_type() {
  return new_type(TY_INT, 4, 4);
}

Type* new_uint_type() {
  Type* type = new_int_type();
  type->is_unsigned = true;
  return type;
}

Type* new_long_type() {
  return new_type(TY_LONG, 8, 8);
}

Type* new_ulong_type() {
  Type* type = new_long_type();
  type->is_unsigned = true;
  return type;
}

Type* new_float_type() {
  return new_type(TY_FLOAT, 4, 4);
}

Type* new_double_type() {
  return new_type(TY_DOUBLE, 8, 8);
}

Type* new_ptr_type(Type* base) {
  Type* ptr = new_type(TY_PTR, 8, 8);
  ptr->base = base;
  ptr->is_unsigned = true;
  return ptr;
}

Type* new_func_type(Type* return_type, Type* params, bool is_variadic) {
  Type* func = new_type(TY_FUNC, 0, 0);
  func->return_type = return_type;
  func->params = params;
  func->is_variadic = is_variadic;
  return func;
}

Type* new_array_type(Type* base, int len) {
  Type* arr = new_type(TY_ARRAY, base->size * len, len >= 16 ? MAX(base->alignment, 16) : base->alignment);
  arr->base = base;
  arr->len = len;
  return arr;
}

Type* new_chars_type(int len) {
  return new_array_type(new_char_type(), len);
}

static Type* new_composite_type(TypeKind kind, int size, int alignment, Member* mems) {
  Type* type = new_type(kind, align_up(size, alignment), alignment);
  type->members = mems;
  return type;
}

Type* new_struct_type(int size, int alignment, Member* mems) {
  return new_composite_type(TY_STRUCT, size, alignment, mems);
}

Type* new_union_type(int size, int alignment, Member* mems) {
  return new_composite_type(TY_UNION, size, alignment, mems);
}

Type* copy_type(Type* src) {
  Type* type = calloc(1, sizeof(Type));
  *type = *src;
  return type;
}

Type* copy_type_with_name(Type* src, char* name) {
  Type* type = copy_type(src);
  type->name = name;
  return type;
}

static Member* copy_member(Member* src) {
  Member* mem = calloc(1, sizeof(Member));
  *mem = *src;
  mem->next = NULL;
  return mem;
}

Type* copy_composite_type(Type* src, TypeKind kind) {
  Member head = {};
  Member* cur = &head;
  for (Member* mem = src->members; mem; mem = mem->next) {
    cur = cur->next = copy_member(mem);
  }

  return new_composite_type(kind, src->size, src->alignment, head.next);
}

Type* get_common_type(Type* a, Type* b) {
  if (a->kind == TY_FUNC) {
    return new_ptr_type(a);
  }
  if (b->kind == TY_FUNC) {
    return new_ptr_type(b);
  }

  if (a->base) {
    return new_ptr_type(a->base);
  }

  if (a->kind == TY_DOUBLE || b->kind == TY_DOUBLE) {
    return new_double_type();
  }
  if (a->kind == TY_FLOAT || b->kind == TY_FLOAT) {
    return new_float_type();
  }

  Type* x = a;
  Type* y = b;

  if (x->size < 4) {
    x = new_int_type();
  }
  if (y->size < 4) {
    y = new_int_type();
  }

  if (x->size != y->size) {
    return x->size > y->size ? x : y;
  }

  return y->is_unsigned ? y : x;
}

Type* deref_ptr_type(Type* type) {
  return type->kind == TY_PTR ? type->base : type;
}

Type* inherit_decl(Type* dst, Type* src) {
  dst->ident = src->ident;
  dst->name = src->name;
  return dst;
}

bool is_pointable_type(Type* type) {
  return type->kind == TY_PTR || type->kind == TY_ARRAY;
}

bool is_int_type(Type* type) {
  return type->kind == TY_BOOL || type->kind == TY_CHAR || type->kind == TY_SHORT || type->kind == TY_INT
         || type->kind == TY_LONG;
}

bool is_float_type(Type* type) {
  return type->kind == TY_DOUBLE || type->kind == TY_FLOAT;
}

bool is_numeric_type(Type* type) {
  return is_int_type(type) || is_float_type(type);
}

bool is_composite_type(Type* type) {
  return type->kind == TY_STRUCT || type->kind == TY_UNION;
}

bool is_type_compatible_with(Type* type, Type* other) {
  if (type->kind != other->kind) {
    return false;
  }

  switch (type->kind) {
    case TY_VOID:
      return true;
    case TY_CHAR:
    case TY_SHORT:
    case TY_INT:
    case TY_LONG:
      return type->is_unsigned == other->is_unsigned;
    case TY_FLOAT:
    case TY_DOUBLE:
      return true;
    case TY_STRUCT:
    case TY_UNION: {
      if (!type->is_defined || !other->is_defined) {
        return false;
      }

      Member* mem = type->members;
      Member* other_mem = other->members;
      for (; mem && other_mem; mem = mem->next, other_mem = other_mem->next) {
        if (!is_type_compatible_with(mem->type, other_mem->type)) {
          return false;
        }
      }
      return mem == NULL && other_mem == NULL;
    }
    case TY_PTR:
      return is_type_compatible_with(type->base, other->base);
    case TY_FUNC: {
      if (!is_type_compatible_with(type->return_type, other->return_type)) {
        fprintf(stderr, "  is compatible: 0\n");
        return false;
      }

      if (type->is_variadic != other->is_variadic) {
        return false;
      }

      Type* param = type->params;
      Type* other_param = other->params;
      for (; param && other_param; param = param->next, other_param = other_param->next) {
        if (!is_type_compatible_with(param, other_param)) {
          return false;
        }
      }
      return param == NULL && other_param == NULL;
    }
    case TY_ARRAY:
      if (!is_type_compatible_with(type->base, other->base)) {
        return false;
      }

      return (type->len < 0 && other->len < 0) || (type->len == other->len);
    default:
      return false;
  }
}