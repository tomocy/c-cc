#include "adapter.h"

int _Alignas(512) g1;
int _Alignas(512) g2;
char g3;
int g4;
long g5;
char g6;
int g7[30];

int main() {
  ASSERT(1, _Alignof(char));
  ASSERT(2, _Alignof(short));
  ASSERT(4, _Alignof(int));
  ASSERT(8, _Alignof(long));
  ASSERT(8, _Alignof(long long));
  ASSERT(1, _Alignof(char[3]));
  ASSERT(4, _Alignof(int[3]));
  ASSERT(1, _Alignof(struct {
    char a;
    char b;
  }[2]));
  ASSERT(8, _Alignof(struct {
    char a;
    long b;
  }[2]));

  ASSERT(1, ({
    _Alignas(char) char x;
    _Alignas(char) char y;
    &x - &y;
  }));
  ASSERT(8, ({
    _Alignas(long) char x;
    _Alignas(long) char y;
    &x - &y;
  }));
  ASSERT(32, ({
    _Alignas(32) char x;
    _Alignas(32) char y;
    &x - &y;
  }));
  ASSERT(32, ({
    _Alignas(32) int* x;
    _Alignas(32) int* y;
    ((char*)&x) - ((char*)&y);
  }));
  ASSERT(16, ({
    struct {
      _Alignas(16) char x, y;
    } a;
    &a.y - &a.x;
  }));
  ASSERT(8, ({
    struct T {
      _Alignas(8) char a;
    };
    _Alignof(struct T);
  }));

  ASSERT(0, (long)(char*)&g1 % 512);
  ASSERT(0, (long)(char*)&g2 % 512);
  ASSERT(0, (long)(char*)&g4 % 4);
  ASSERT(0, (long)(char*)&g5 % 8);

  ASSERT(1, ({
    char x;
    // NOLINTNEXTLINE
    _Alignof(x);
  }));
  ASSERT(4, ({
    int x;
    // NOLINTNEXTLINE
    _Alignof(x);
  }));
  ASSERT(1, ({
    char x;
    // NOLINTNEXTLINE
    _Alignof x;
  }));
  ASSERT(4, ({
    int x;
    // NOLINTNEXTLINE
    _Alignof x;
  }));

  ASSERT(1, _Alignof(char) << 31 >> 31);
  ASSERT(1, _Alignof(char) << 63 >> 63);
  ASSERT(1, ({
    char x;
    // NOLINTNEXTLINE
    _Alignof(x) << 63 >> 63;
  }));

  ASSERT(0, ({
    int x[10];
    (unsigned long)&x % 16;
  }));
  ASSERT(4, ({
    int x[10];
    _Alignof(x);  // NOLINT
  }));
  ASSERT(0, ({
    int x[11];
    (unsigned long)&x % 16;
  }));
  ASSERT(4, ({
    int x[11];
    _Alignof(x);  // NOLINT
  }));
  ASSERT(0, ({
    char x[100];
    (unsigned long)&x % 16;
  }));
  ASSERT(1, ({
    char x[100];
    _Alignof(x);  // NOLINT
  }));
  ASSERT(0, ({
    char x[101];
    (unsigned long)&x % 16;
  }));
  ASSERT(1, ({
    char x[101];
    _Alignof(x);  // NOLINT
  }));
  ASSERT(0, ({ (unsigned long)&g7 % 16; }));
  ASSERT(4, ({
    _Alignof(g7);  // NOLINT
  }));

  ok();
}
