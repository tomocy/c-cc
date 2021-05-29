#include "adapter.h"

int main() {
  // NOLINTNEXTLINE
  { const x; }
  // NOLINTNEXTLINE
  { int const x; }
  // NOLINTNEXTLINE
  { const int x; }
  // NOLINTNEXTLINE
  { const int const const x; }
  ASSERT(5, ({
    // NOLINTNEXTLINE
    const x = 5;
    x;
  }));
  ASSERT(8, ({
    // NOLINTNEXTLINE
    const x = 8;
    // NOLINTNEXTLINE
    int* const y = &x;
    *y;
  }));
  ASSERT(6, ({
    // NOLINTNEXTLINE
    const x = 6;
    // NOLINTNEXTLINE
    *(const* const)&x;
  }));

  ok();
}