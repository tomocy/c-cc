#include "include1.h"

void assert(char* name, int expected, int actual);
void ok(void);

#
#

#if 0
assert("unreachable", 1, 0);
#if 1
assert("unreachable", 1, 0);
#endif
#if nested
assert("unreachable", 1, 0);
#endif
assert("unreachable", 1, 0);
#include "/no/such/file"
assert("unreachable", 1, 0);
#endif

#if 1
int g1 = 5;
#endif

#if 1
#if 0
assert("unreachable", 1, 0);
#if 1
assert("unreachable", 1, 0);
foo bar
#endif
assert("unreachable", 1, 0);
#endif
int g2 = 3;
#endif

#if 1 - 1
assert("unreachable", 1, 0);
int g3 = 5;
#if 1
assert("unreachable", 1, 0);
#endif
#if 1
assert("unreachable", 1, 0);
#if 1
assert("unreachable", 1, 0);
#else
assert("unreachable", 1, 0);
#endif
assert("unreachable", 1, 0);
#else
assert("unreachable", 1, 0);
#if 1
assert("unreachable", 1, 0);
#else
assert("unreachable", 1, 0);
#endif
assert("unreachable", 1, 0);
#endif
assert("unreachable", 1, 0);
#if 0
assert("unreachable", 1, 0);
#else
assert("unreachable", 1, 0);
#endif
assert("unreachable", 1, 0);
int g3 = 6;
#else
#if 1
int g3 = 8;
#endif
#endif

int mm = 0;

int ret3(void) {
  return 3;
}

int main() {
  assert("include1", 5, include1);
  assert("include2", 7, include2);

  assert("g1", 5, g1);
  assert("g2", 3, g2);
  assert("g3", 8, g3);

#if 1
  mm = 2;
#else
  assert("unreachable", 1, 0);
  mm = 3;
#endif
  assert("mm", 2, mm);

#if 0
  assert("unreachable", 1, 0);
  mm = 1;
#elif 0
  assert("unreachable", 1, 0);
  mm = 2;
#elif 3 + 5
  mm = 3;
#elif 1 * 5
  assert("unreachable", 1, 0);
  mm = 4;
#endif
  assert("mm", 3, mm);

#if 1 + 5
  mm = 1;
#elif 1
  assert("unreachable", 1, 0);
  mm = 2;
#elif 3
  assert("unreachable", 1, 0);
  mm = 2;
#endif
  assert("mm", 1, mm);

#if 0
  assert("unreachable", 1, 0);
  mm = 1;
#elif 1
#if 1
  mm = 2;
#else
  assert("unreachable", 1, 0);
  mm = 3;
#endif
#else
  assert("unreachable", 1, 0);
  mm = 5;
#endif
  assert("mm", 2, mm);

  // NOLINTNEXTLINE
  int M1 = 5;

#define M1 3
  assert("M1", 3, M1);
  // NOLINTNEXTLINE
#define M1 4
  assert("M1", 4, M1);

// NOLINTNEXTLINE
#define M1 3 + 4 +
  assert("M1 5", 12, M1 5);

// NOLINTNEXTLINE
#define M1 3 + 4
  assert("M1 * 5", 23, M1 * 5);

#define ASSERT_ assert(
#define if 5
#define five "5"
#define END )
  ASSERT_ five, 5, if END;

#undef ASSERT_
#undef if
#undef five
#undef END
  if (1) {
    assert("1", 1, 1);
  }
  if (0) {
    assert("unreachable", 1, 0);
  }

#define M 5
#if M
  mm = 5;
#else
  mm = 6;
#endif
  assert("mm", 5, mm);

#define M 5
#if M - 5
  mm = 6;
#elif M
  mm = 8;
#endif
  assert("mm", 8, mm);

  int M2 = 6;
#define M2 M2 + 3
  assert("M2", 9, M2);

#define M3 M2 + 3
  assert("M3", 12, M3);

  int M4 = 3;
#define M4 M5 * 5
#define M5 M4 + 2
  assert("M4", 13, M4);

#ifdef M6
  mm = 5;
  assert("unreachable", 1, 0);
#else
  mm = 3;
#endif
  assert("mm", 3, mm);

#define M6
#ifdef M6
  mm = 5;
#else
  mm = 3;
  assert("unreachable", 1, 0);
#endif
  assert("mm", 5, mm);

#ifndef M7
  mm = 3;
#else
  mm = 5;
  assert("unreachable", 1, 0);
#endif
  assert("mm", 3, mm);

#define M7
#ifndef M7
  mm = 3;
  assert("unreachable", 1, 0);
#else
  mm = 5;
#endif
  assert("mm", 5, mm);

#if 0
#ifdef NO_SUCH_MACRO
  assert("unreachable", 1, 0);
#endif
#ifndef NO_SUCH_MACRO
  assert("unreachable", 1, 0);
#endif
  assert("unreachable", 1, 0);
#else
  assert("1", 1, 1);
#endif

// NOLINTNEXTLINE
#define M7() 1
  // NOLINTNEXTLINE
  int M7 = 5;
  assert("M7()", 1, M7());
  assert("M7", 5, M7);

// NOLINTNEXTLINE
#define M7 ()
  assert("ret3 M7", 3, ret3 M7);

#define ARGS(a, b, c) 1
  assert("ARGS(1, 2, 3)", 1, ARGS(1, 2, 3));

#define ADD(x, y) x + y
#define SUB(x, y) x - y

#define M8(x, y) x + y
  assert("M8(3, 4)", 7, M8(3, 4));

// NOLINTNEXTLINE
#define M8(x, y) x* y
  assert("M8(3 + 4, 4 + 5)", 24, M8(3 + 4, 4 + 5));

// NOLINTNEXTLINE
#define M8(x, y) (x) * (y)
  assert("M8(ADD(SUB(6, 3), 4), 4 + 5)", 63, M8(ADD(SUB(6, 3), 4), 4 + 5));

// NOLINTNEXTLINE
#define M8(x, y) x y
  assert("M8(, 4+5)", 9, M8(, 4 + 5));

  ok();
}