#include "include1.h"

void assert(char* name, int expected, int actual);
void ok(void);

#
#

#if 0
#if 1
#endif
#if nested
#endif
#include "/no/such/file"
assert(0, 1, "1");
#endif

#if 1
int g1 = 5;
#endif

#if 1
#if 0
#if 1
    foo bar
#endif
#endif
int g2 = 3;
#endif

#if 1 - 1
int g3 = 5;
#if 1
#endif
#if 1
#if 1
#else
#endif
#else
#if 1
#else
#endif
#endif
#if 0
#else
#endif
int g3 = 6;
#else
#if 1
int g3 = 8;
#endif
#endif

int main() {
  assert("include1", 5, include1);
  assert("include2", 7, include2);

  assert("g1", 5, g1);
  assert("g2", 3, g2);
  assert("g3", 8, g3);

  ok();
}