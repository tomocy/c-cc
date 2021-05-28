#include "adapter.h"

int main() {
  ASSERT(131585, (int)8590066177);
  ASSERT(513, (short)8590066177);
  ASSERT(1, (char)8590066177);
  ASSERT(1, (long)1);
  ASSERT(0, (long)&*(int*)0);
  ASSERT(513, ({
    int x = 512;
    *(char*)&x = 1;
    x;
  }));
  ASSERT(5, ({
    int x = 5;
    long y = (long)&x;
    // NOLINTNEXTLINE
    *(int*)y;
  }));

  (void)1;

  ASSERT(-1, (char)255);
  ASSERT(-1, (signed char)255);
  ASSERT(255, (unsigned char)255);
  ASSERT(-1, (short)65535);
  ASSERT(65535, (unsigned short)65535);
  ASSERT(-1, (int)0xffffffff);
  ASSERT(0xffffffff, (unsigned)0xffffffff);

  ASSERT(1, -1 < 1);
  ASSERT(0, -1 < (unsigned)1);
  ASSERT(254, (char)127 + (char)127);
  ASSERT(65534, (short)32767 + (short)32767);
  ASSERT(-1, -1 >> 1);
  // NOLINTNEXTLINE
  ASSERT(-1, (unsigned long)-1);
  ASSERT(2147483647, ((unsigned)-1) >> 1);
  ASSERT(-50, (-100) / 2);
  ASSERT(2147483598, ((unsigned)-100) / 2);
  // NOLINTNEXTLINE
  ASSERT(9223372036854775758, ((unsigned long)-100) / 2);
  ASSERT(0, ((long)-1) / (unsigned)100);
  ASSERT(-2, (-100) % 7);
  ASSERT(2, ((unsigned)-100) % 7);
  ASSERT(6, ((unsigned long)-100) % 9);

  ASSERT(65535, (int)(unsigned short)65535);
  ASSERT(65535, ({
    unsigned short x = 65535;
    x;
  }));
  ASSERT(65535, ({
    unsigned short x = 65535;
    (int)x;
  }));

  ASSERT(-1, ({
    typedef short T;
    // NOLINTNEXTLINE
    T x = 65535;
    (int)x;
  }));
  ASSERT(65535, ({
    typedef unsigned short T;
    T x = 65535;
    (int)x;
  }));

  // from i8
  ASSERT(-1, (char)(char)255);
  ASSERT(-1, (short)(char)255);
  ASSERT(-1, (int)(char)255);
  ASSERT(-1, (long)(char)255);
  ASSERT(255, (unsigned char)(char)255);
  ASSERT(65535, (unsigned short)(char)255);
  // NOLINTNEXTLINE
  ASSERT(4294967295, (unsigned int)(char)255);
  // NOLINTNEXTLINE
  ASSERT(18446744073709551615, (unsigned long)(char)255);
  // from i16
  ASSERT(-1, (char)(short)65535);
  ASSERT(-1, (short)(short)65535);
  ASSERT(-1, (int)(short)65535);
  ASSERT(-1, (long)(short)65535);
  ASSERT(255, (unsigned char)(short)65535);
  ASSERT(65535, (unsigned short)(short)65535);
  // NOLINTNEXTLINE
  ASSERT(4294967295, (unsigned int)(short)65535);
  // NOLINTNEXTLINE
  ASSERT(18446744073709551615, (unsigned long)(short)65535);
  // from i32
  ASSERT(-1, (char)(int)4294967295);
  ASSERT(-1, (short)(int)4294967295);
  ASSERT(-1, (int)(int)4294967295);
  ASSERT(-1, (long)(int)4294967295);
  ASSERT(255, (unsigned char)(int)4294967295);
  ASSERT(65535, (unsigned short)(int)4294967295);
  // NOLINTNEXTLINE
  ASSERT(4294967295, (unsigned int)(int)4294967295);
  // NOLINTNEXTLINE
  ASSERT(18446744073709551615, (unsigned long)(int)4294967295);
  // from i64
  // NOLINTNEXTLINE
  ASSERT(-1, (char)(long)18446744073709551615);
  // NOLINTNEXTLINE
  ASSERT(-1, (short)(long)18446744073709551615);
  // NOLINTNEXTLINE
  ASSERT(-1, (int)(long)18446744073709551615);
  // NOLINTNEXTLINE
  ASSERT(-1, (long)(long)18446744073709551615);
  // NOLINTNEXTLINE
  ASSERT(255, (unsigned char)(long)18446744073709551615);
  // NOLINTNEXTLINE
  ASSERT(65535, (unsigned short)(long)18446744073709551615);
  // NOLINTNEXTLINE
  ASSERT(4294967295, (unsigned int)(long)18446744073709551615);
  // NOLINTNEXTLINE
  ASSERT(18446744073709551615, (unsigned long)(long)18446744073709551615);

  ok();
}