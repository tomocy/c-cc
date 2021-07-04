#include "adapter.h"

char* asm_fn1(void) {
  asm(
    "mov rax, 50\n\t"
    "mov rsp, rbp\n\t"
    "pop rbp\n\t"
    "ret");
}  // NOLINT

char* asm_fn2(void) {
  asm inline volatile(
    "mov rax, 55\n\t"
    "mov rsp, rbp\n\t"
    "pop rbp\n\t"
    "ret");
}  // NOLINT

int main() {
  ASSERT(50, asm_fn1());  // NOLINT
  ASSERT(55, asm_fn2());  // NOLINT

  ok();
}