#include "adapter.h"

int main() {
  ASSERT(35, (float)(char)35);
  ASSERT(35, (float)(short)35);
  ASSERT(35, (float)(int)35);
  ASSERT(35, (float)(long)35);
  ASSERT(35, (float)(unsigned char)35);
  ASSERT(35, (float)(unsigned short)35);
  ASSERT(35, (float)(unsigned int)35);
  ASSERT(35, (float)(unsigned long)35);

  ASSERT(35, (double)(char)35);
  ASSERT(35, (double)(short)35);
  ASSERT(35, (double)(int)35);
  ASSERT(35, (double)(long)35);
  ASSERT(35, (double)(unsigned char)35);
  ASSERT(35, (double)(unsigned short)35);
  ASSERT(35, (double)(unsigned int)35);
  ASSERT(35, (double)(unsigned long)35);

  ASSERT(35, (char)(float)35);
  ASSERT(35, (short)(float)35);
  ASSERT(35, (int)(float)35);
  ASSERT(35, (long)(float)35);
  ASSERT(35, (unsigned char)(float)35);
  ASSERT(35, (unsigned short)(float)35);
  ASSERT(35, (unsigned int)(float)35);
  ASSERT(35, (unsigned long)(float)35);

  ASSERT(35, (char)(double)35);
  ASSERT(35, (short)(double)35);
  ASSERT(35, (int)(double)35);
  ASSERT(35, (long)(double)35);
  ASSERT(35, (unsigned char)(double)35);
  ASSERT(35, (unsigned short)(double)35);
  ASSERT(35, (unsigned int)(double)35);
  ASSERT(35, (unsigned long)(double)35);

  ASSERT(-2147483648, (double)(unsigned long)(long)-1);

  ASSERT(1, 2e3 == 2e3);
  ASSERT(0, 2e3 == 2e5);
  ASSERT(1, 2.0 == 2);
  ASSERT(0, 5.1 < 5);
  ASSERT(0, 5.0 < 5);
  ASSERT(1, 4.9 < 5);
  ASSERT(0, 5.1 <= 5);
  ASSERT(1, 5.0 <= 5);
  ASSERT(1, 4.9 <= 5);

  ASSERT(1, 2e3F == 2e3);
  ASSERT(0, 2e3F == 2e5);
  ASSERT(1, 2.0F == 2);
  ASSERT(0, 5.1F < 5);
  ASSERT(0, 5.0F < 5);
  ASSERT(1, 4.9F < 5);
  ASSERT(0, 5.1F <= 5);
  ASSERT(1, 5.0F <= 5);
  ASSERT(1, 4.9F <= 5);

  ASSERT(6, 2.3 + 3.8);
  ASSERT(-1, 2.3 - 3.8);
  ASSERT(-3, -3.8);  // NOLINT
  ASSERT(13, 3.3 * 4);
  ASSERT(2, 5.0 / 2);

  ASSERT(6, 2.3F + 3.8F);
  ASSERT(6, 2.3F + 3.8);
  ASSERT(-1, 2.3F - 3.8);
  ASSERT(-3, -3.8F);  // NOLINT
  ASSERT(13, 3.3F * 4);
  ASSERT(2, 5.0F / 2);

  ASSERT(0, 0.0 / 0.0 == 0.0 / 0.0);
  ASSERT(1, 0.0 / 0.0 != 0.0 / 0.0);

  ASSERT(0, 0.0 / 0.0 < 0);
  ASSERT(0, 0.0 / 0.0 <= 0);
  ASSERT(0, 0.0 / 0.0 > 0);
  ASSERT(0, 0.0 / 0.0 >= 0);

  ASSERT(0, !3.);  // NOLINT
  ASSERT(1, !0.);
  ASSERT(0, !3.f);  // NOLINT
  ASSERT(1, !0.f);  // NOLINT

  ASSERT(5, 0.0 ? 3 : 5);
  ASSERT(3, 1.2 ? 3 : 5);  // NOLINT

  ASSERT(0, ({
    long double a = 10;
    long double b = 5;
    a == b;
  }));
  ASSERT(1, ({
    long double a = 10;
    long double b = 5;
    a != b;
  }));
  ASSERT(0, ({
    long double a = 10;
    long double b = 5;
    a < b;
  }));
  ASSERT(0, ({
    long double a = 10;
    long double b = 5;
    a <= b;
  }));
  ASSERT(15, ({
    long double a = 10;
    long double b = 5;
    a + b;
  }));
  ASSERT(5, ({
    long double a = 10;
    long double b = 5;
    a - b;
  }));
  ASSERT(50, ({
    long double a = 10;
    long double b = 5;
    (a) * b;
  }));
  ASSERT(2, ({
    long double a = 10;
    long double b = 5;
    a / b;
  }));

  ok();
}