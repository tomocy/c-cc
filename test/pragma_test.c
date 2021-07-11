#include "adapter.h"

#pragma once

#include "pragma_test.c"

#pragma ignored

static int x = 42;

int main() {
  ASSERT(x, 42);
  ok();
}