#include "include1.h"

void assert(char* name, int expected, int actual);
void ok(void);

#

/* */ #

int main() {
  assert("include1", 5, include1);
  assert("include2", 7, include2);

  ok();
}