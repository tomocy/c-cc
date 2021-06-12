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

  ok();
}