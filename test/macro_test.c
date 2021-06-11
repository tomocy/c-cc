#include "include1.h"

void assert(char* name, int expected, int actual);
void ok(void);

#

/* */ #

#if 0
#if 1
#endif
#if nested
#endif
#include "/no/such/file"
assert(0, 1, "1");
#endif

#if 1
  int m
  = 5;
#endif

int main() {
  assert("include1", 5, include1);
  assert("include2", 7, include2);
  assert("m", 5, m);

  ok();
}