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

  ok();
}