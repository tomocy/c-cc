#include "adapter.h"

// NOLINTNEXTLINE
_Noreturn noreturn_fn(int restrict x) {
  exit(0);
}

void funcy_type(int arg[restrict static 3]) {}

int main() {
  // NOLINTNEXTLINE
  { volatile x; }
  // NOLINTNEXTLINE
  { int volatile x; }
  // NOLINTNEXTLINE
  { volatile int x; }
  // NOLINTNEXTLINE
  { volatile int volatile volatile x; }
  // NOLINTNEXTLINE
  { int volatile* volatile volatile x; }
  // NOLINTNEXTLINE
  { auto** restrict __restrict __restrict__ const volatile* x; }

  ok();
  return 0;
}