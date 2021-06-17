#include "adapter.h"
#include "include1.h"

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

int dbl(int x) {
  return x * x;
}

int tri(int x) {
  return x * x * x;
}

int main() {
  ASSERT(5, include1);
  ASSERT(7, include2);

  ASSERT(5, g1);
  ASSERT(3, g2);
  ASSERT(8, g3);

#if 1
  mm = 2;
#else
  assert("unreachable", 1, 0);
  mm = 3;
#endif
  ASSERT(2, mm);

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
  ASSERT(3, mm);

#if 1 + 5
  mm = 1;
#elif 1
  assert("unreachable", 1, 0);
  mm = 2;
#elif 3
  assert("unreachable", 1, 0);
  mm = 2;
#endif
  ASSERT(1, mm);

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
  ASSERT(2, mm);

  // NOLINTNEXTLINE
  int M1 = 5;

#define M1 3
  ASSERT(3, M1);
  // NOLINTNEXTLINE
#define M1 4
  ASSERT(4, M1);

// NOLINTNEXTLINE
#define M1 3 + 4 +
  ASSERT(12, M1 5);

// NOLINTNEXTLINE
#define M1 3 + 4
  ASSERT(23, M1 * 5);

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
    ASSERT(1, 1);
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
  ASSERT(5, mm);

#define M 5
#if M - 5
  mm = 6;
#elif M
  mm = 8;
#endif
  ASSERT(8, mm);

  // NOLINTNEXTLINE
  int M2 = 6;
#define M2 M2 + 3
  ASSERT(9, M2);

#define M3 M2 + 3
  ASSERT(12, M3);

  // NOLINTNEXTLINE
  int M4 = 3;
#define M4 M5 * 5
#define M5 M4 + 2
  ASSERT(13, M4);

#ifdef M6
  mm = 5;
  assert("unreachable", 1, 0);
#else
  mm = 3;
#endif
  ASSERT(3, mm);

#define M6
#ifdef M6
  mm = 5;
#else
  mm = 3;
  assert("unreachable", 1, 0);
#endif
  ASSERT(5, mm);

#ifndef M7
  mm = 3;
#else
  mm = 5;
  assert("unreachable", 1, 0);
#endif
  ASSERT(3, mm);

#define M7
#ifndef M7
  mm = 3;
  assert("unreachable", 1, 0);
#else
  mm = 5;
#endif
  ASSERT(5, mm);

#if 0
#ifdef NO_SUCH_MACRO
  assert("unreachable", 1, 0);
#endif
#ifndef NO_SUCH_MACRO
  assert("unreachable", 1, 0);
#endif
  assert("unreachable", 1, 0);
#else
  ASSERT(1, 1);
#endif

  // NOLINTNEXTLINE
#define M7() 1
  // NOLINTNEXTLINE
  int M7 = 5;
  ASSERT(1, M7());
  ASSERT(5, M7);

// NOLINTNEXTLINE
#define M7 ()
  ASSERT(3, ret3 M7);

#define ARGS(a, b, c) 1
  ASSERT(1, ARGS(1, 2, 3));

#define ADD(x, y) x + y
#define SUB(x, y) x - y

#define M8(x, y) x + y
  ASSERT(7, M8(3, 4));

  // NOLINTNEXTLINE
#define M8(x, y) x* y
  ASSERT(24, M8(3 + 4, 4 + 5));

// NOLINTNEXTLINE
#define M8(x, y) (x) * (y)
  ASSERT(63, M8(ADD(SUB(6, 3), 4), 4 + 5));

// NOLINTNEXTLINE
#define M8(x, y) x y
  ASSERT(9, M8(, 4 + 5));

//  NOLINTNEXTLINE
#define M8(x, y) x* y
  ASSERT(20, M8((2 + 3), 4));

// NOLINTNEXTLINE
#define M8(x, y) x* y
  // NOLINTNEXTLINE
  ASSERT(12, M8((2, 3), 4));

#define dbl(x) M10(x) * x
#define M10(x) dbl(x) + dbl(3)
  ASSERT(22, dbl(2));

#define M11(x) #x
  ASSERT('a', M11(a !b  `"" c)[0]);
  ASSERT(' ', M11(a !b  `"" c)[1]);
  ASSERT('!', M11(a !b  `"" c)[2]);
  ASSERT('b', M11(a !b  `"" c)[3]);
  ASSERT(' ', M11(a !b  `"" c)[4]);
  ASSERT('`', M11(a !b  `"" c)[5]);
  ASSERT('"', M11(a !b  `"" c)[6]);
  ASSERT('"', M11(a !b  `"" c)[7]);
  ASSERT(' ', M11(a !b  `"" c)[8]);
  ASSERT('c', M11(a !b  `"" c)[9]);
  ASSERT(0, M11(a !b  `"" c)[10]);

#define paste(x, y) x##y
  ASSERT(15, paste(1, 5));
  ASSERT(255, paste(0, xff));
  ASSERT(3, ({
    int foobar = 3;
    paste(foo, bar);
  }));
  ASSERT(5, paste(5, ));
  ASSERT(5, paste(, 5));

#define paste2(x) x##5
  ASSERT(26, paste2(1 + 2));

#define paste3(x) 2##x
  ASSERT(23, paste3(1 + 2));

#define paste4(x, y, z) x##y##z
  ASSERT(123, paste4(1, 2, 3));

#define paste5(x, y, z) 1##2##3
  ASSERT(123, paste5(1, 1, 1));

#define M12
#if defined(M12)
  mm = 3;
#else
  assert("unreachable", 1, 0);
  mm = 4;
#endif
  ASSERT(3, mm);

#define M12
#if defined M12
  mm = 3;
#else
  assert("unreachable", 1, 0);
  mm = 4;
#endif
  ASSERT(3, mm);

#if defined(M12) - 1
  assert("unreachable", 1, 0);
  mm = 3;
#else
  mm = 4;
#endif
  ASSERT(4, mm);

#if defined(NO_SUCH_MACRO)
  assert("unreachable", 1, 0);
  mm = 3;
#else
  mm = 4;
#endif
  ASSERT(4, mm);

#if no_such_symbol == 0
  mm = 5;
#else
  assert("unreachable", 1, 0);
  m = 6;
#endif
  ASSERT(5, mm);

#define STR(x) #x
// NOLINTNEXTLINE
#define M12(x) STR(x)
#define M13(x) M12(foo.x)
  ASSERT(0, strcmp(M13(bar), "foo.bar"));

// clang-format off
// NOLINTNEXTLINE
#define M13(x) M12(foo. x)
  // clang-format on
  ASSERT(0, strcmp(M13(bar), "foo. bar"));

// NOLINTNEXTLINE
#define M12 foo
// NOLINTNEXTLINE
#define M13(x) STR(x)
#define M14(x) M13(x.M12)
  ASSERT(0, strcmp(M14(bar), "bar.foo"));

// clang-format off
// NOLINTNEXTLINE
#define M14(x) M13(x. M12)
  // clang-format on
  ASSERT(0, strcmp(M14(bar), "bar. foo"));

  assert("sizeof(char)", 1, size\
of(char));

#include "include3.h"
  ASSERT(3, foo);

#include "include4.h"
  ASSERT(4, foo);

// NOLINTNEXTLINE
#define M13 "include3.h"
#include M13
  ASSERT(3, foo);

// clang-format off
// NOLINTNEXTLINE
#define M13 < include4.h
// NOLINTNEXTLINE
#include M13 >
  // clang-format on
  ASSERT(4, foo);

#undef foo

  ok();
}