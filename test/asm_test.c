#include "adapter.h"

char* asm_fn1(void) {
  asm(
    "mov $50, %rax\n\t"
    "mov %rbp, %rsp\n\t"
    "pop %rbp\n\t"
    "ret");
}  // NOLINT

char* asm_fn2(void) {
  asm inline volatile(
    "mov $55, %rax\n\t"
    "mov %rbp, %rsp\n\t"
    "pop %rbp\n\t"
    "ret");
}  // NOLINT

int main() {
  ASSERT(50, asm_fn1());  // NOLINT
  ASSERT(55, asm_fn2());  // NOLINT

  ok();
}