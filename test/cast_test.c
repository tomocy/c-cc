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

  ASSERT(0, (_Bool)0.0);
  ASSERT(1, (_Bool)0.1);
  ASSERT(3, (char)3.0);
  ASSERT(1000, (short)1000.3);
  ASSERT(3, (int)3.99);
  ASSERT(2000000000000000, (long)2e15);  // NOLINT
  ASSERT(3, (float)3.5);
  ASSERT(5, (double)(float)5.5);
  ASSERT(3, (float)3);
  ASSERT(3, (double)3);
  ASSERT(3, (float)3L);
  ASSERT(3, (double)3L);

  // from i8
  ASSERT(-1, (char)(char)-1);
  ASSERT(-1, (short)(char)-1);
  ASSERT(-1, (int)(char)-1);
  ASSERT(-1, (long)(char)-1);
  ASSERT(255, (unsigned char)(char)-1);
  ASSERT(65535, (unsigned short)(char)-1);
  ASSERT(4294967295, (unsigned int)(char)-1);             // NOLINTN
  ASSERT(18446744073709551615, (unsigned long)(char)-1);  // NOLINTN
  ASSERT(-1, (float)(char)-1);
  ASSERT(-1, (double)(char)-1);
  ASSERT(-1, (long double)(char)-1);
  // from i16
  ASSERT(-1, (char)(short)-1);
  ASSERT(-1, (short)(short)-1);
  ASSERT(-1, (int)(short)-1);
  ASSERT(-1, (long)(short)-1);
  ASSERT(255, (unsigned char)(short)-1);
  ASSERT(65535, (unsigned short)(short)-1);
  ASSERT(4294967295, (unsigned int)(short)-1);             // NOLINTN
  ASSERT(18446744073709551615, (unsigned long)(short)-1);  // NOLINTN
  ASSERT(-1, (float)(short)-1);
  ASSERT(-1, (double)(short)-1);
  ASSERT(-1, (long double)(short)-1);
  // from i32
  ASSERT(-1, (char)(int)-1);
  ASSERT(-1, (short)(int)-1);
  ASSERT(-1, (int)(int)-1);
  ASSERT(-1, (long)(int)-1);
  ASSERT(255, (unsigned char)(int)-1);
  ASSERT(65535, (unsigned short)(int)-1);
  ASSERT(4294967295, (unsigned int)(int)-1);             // NOLINTN
  ASSERT(18446744073709551615, (unsigned long)(int)-1);  // NOLINTN
  ASSERT(-1, (float)(int)-1);
  ASSERT(-1, (double)(int)-1);
  ASSERT(-1, (long double)(int)-1);
  // from i64
  ASSERT(-1, (char)(long)-1);
  ASSERT(-1, (short)(long)-1);
  ASSERT(-1, (int)(long)-1);
  ASSERT(-1, (long)(long)-1);
  ASSERT(255, (unsigned char)(long)-1);
  ASSERT(65535, (unsigned short)(long)-1);
  ASSERT(4294967295, (unsigned int)(long)-1);             // NOLINTN
  ASSERT(18446744073709551615, (unsigned long)(long)-1);  // NOLINTN
  ASSERT(-1, (float)(long)-1);
  ASSERT(-1, (double)(long)-1);
  ASSERT(-1, (long double)(long)-1);
  // from u8
  ASSERT(-1, (char)(unsigned char)255);
  ASSERT(255, (short)(unsigned char)255);
  ASSERT(255, (int)(unsigned char)255);
  ASSERT(255, (long)(unsigned char)255);
  ASSERT(255, (unsigned char)(unsigned char)255);
  ASSERT(255, (unsigned short)(unsigned char)255);
  ASSERT(255, (unsigned int)(unsigned char)255);
  ASSERT(255, (unsigned long)(unsigned char)255);
  ASSERT(255, (float)(unsigned char)255);
  ASSERT(255, (double)(unsigned char)255);
  ASSERT(255, (long double)(unsigned char)255);
  // from u16
  ASSERT(-1, (char)(unsigned short)65535);
  ASSERT(-1, (short)(unsigned short)65535);
  ASSERT(65535, (int)(unsigned short)65535);
  ASSERT(65535, (long)(unsigned short)65535);
  ASSERT(255, (unsigned char)(unsigned short)65535);
  ASSERT(65535, (unsigned short)(unsigned short)65535);
  ASSERT(65535, (unsigned int)(unsigned short)65535);
  ASSERT(65535, (unsigned long)(unsigned short)65535);
  ASSERT(65535, (float)(unsigned short)65535);
  ASSERT(65535, (double)(unsigned short)65535);
  ASSERT(65535, (long double)(unsigned short)65535);
  // from u32
  ASSERT(-1, (char)(unsigned int)4294967295);
  ASSERT(-1, (short)(unsigned int)4294967295);
  ASSERT(-1, (int)(unsigned int)4294967295);
  ASSERT(4294967295, (long)(unsigned int)4294967295);  // NOLINT
  ASSERT(255, (unsigned char)(unsigned int)4294967295);
  ASSERT(65535, (unsigned short)(unsigned int)4294967295);
  ASSERT(4294967295, (unsigned int)(unsigned int)4294967295);   // NOLINT
  ASSERT(4294967295, (unsigned long)(unsigned int)4294967295);  // NOLINT
  ASSERT(-2147483648, (float)(unsigned int)4294967295);
  ASSERT(-2147483648, (double)(unsigned int)4294967295);
  ASSERT(-2147483648, (long double)(unsigned int)4294967295);
  // from u64
  ASSERT(-1, (char)(unsigned long)18446744073709551615UL);
  ASSERT(-1, (short)(unsigned long)18446744073709551615UL);
  ASSERT(-1, (int)(unsigned long)18446744073709551615UL);
  ASSERT(-1, (long)(unsigned long)18446744073709551615UL);
  ASSERT(255, (unsigned char)(unsigned long)18446744073709551615UL);
  ASSERT(65535, (unsigned short)(unsigned long)18446744073709551615UL);
  ASSERT(4294967295, (unsigned int)(unsigned long)18446744073709551615UL);             // NOLINT
  ASSERT(18446744073709551615, (unsigned long)(unsigned long)18446744073709551615UL);  // NOLINT
  ASSERT(-1, (float)(unsigned long)18446744073709551615UL);
  ASSERT(-2147483648, (double)(unsigned long)18446744073709551615UL);
  ASSERT(-2147483648, (long double)(unsigned long)18446744073709551615UL);
  // f32
  ASSERT(0, (char)(float)2147483647);
  ASSERT(0, (short)(float)2147483647);
  ASSERT(-2147483648, (int)(float)2147483647);
  ASSERT(-2147483648, (long)(float)2147483647);  // NOLINT
  ASSERT(0, (unsigned char)(float)2147483647);
  ASSERT(0, (unsigned short)(float)2147483647);
  ASSERT(-2147483648, (unsigned int)(float)2147483647);
  ASSERT(-2147483648, (unsigned long)(float)2147483647);
  ASSERT(-2147483648, (float)(float)2147483647);
  ASSERT(-2147483648, (double)(float)2147483647);
  ASSERT(-2147483648, (long double)(float)2147483647);
  // f64
  ASSERT(-1, (char)(double)2147483647);
  ASSERT(-1, (short)(double)2147483647);
  ASSERT(2147483647, (int)(double)2147483647);
  ASSERT(2147483647, (long)(double)2147483647);
  ASSERT(255, (unsigned char)(double)2147483647);
  ASSERT(65535, (unsigned short)(double)2147483647);
  ASSERT(1, (unsigned int)(double)1);
  ASSERT(1, (unsigned long)(double)1);
  ASSERT(-2147483648, (float)(double)2147483647);
  ASSERT(2147483647, (double)(double)2147483647);
  ASSERT(2147483647, (long double)(double)2147483647);
  // f80
  ASSERT(1, (char)(long double)1);
  ASSERT(1, (short)(long double)1);
  ASSERT(1, (int)(long double)1);
  ASSERT(1, (long)(long double)1);
  ASSERT(1, (unsigned char)(long double)1);
  ASSERT(1, (unsigned short)(long double)1);
  ASSERT(1, (unsigned int)(long double)1);
  ASSERT(1, (unsigned long)(long double)1);
  ASSERT(1, (float)(long double)1);
  ASSERT(1, (double)(long double)1);

  ok();
}