#define ASSERT(x, y) assert(#y, x, y)

void assert(char* name, int expected, int actual);
void ok();

int gx;
int gy;
int gxs[4];
int gz = 42;
int ggx = 1, ggy = 2, ggz = 3;

int ret(int x) {
  return x;
  return -1;
}

int add(int a, int b);

int sub(int a, int b) { return a - b; }

int addmul(int a, int b, int c);

int sum(int a, int b, int c, int d, int e, int f);

int ave(int a, int b, int c, int d, int e, int f) {
  return (a + b + c + d + e + f) / 6;
}

void alloc(int** p, int a, int b, int c, int d);

int deref(int* p) { return *p; }

int add_vals(int n, int* vs) {
  int i;
  for (i = 0; i < n; i = i + 1) {
    vs[i] = i;
  }
  return i;
}

int sum_vals(int n, int* vs) {
  int sum;
  sum = 0;
  int i;
  for (i = 0; i < n; i = i + 1) {
    sum = sum + vs[i];
  }
  return sum;
}

int fibo(int n) {
  if (n < 2) {
    return 1;
  }
  return fibo(n - 1) + fibo(n - 2);
}

int sub_long(long a, long b, long c) { return a - b - c; }

long ret_long(int a) { return a; }

int sub_short(short a, short b, short c) { return a - b - c; }

int no_lvars() { return 0; }

int decl();

typedef char MyChar;

typedef int MyInt;

typedef int MyInt2, MyInts[4];

typedef DefaultType;

char int_to_char(int x) { return x; }

int div_long(long a, long b) { return a / b; }

int sum_int_to_char(char a, char b) { return a + b; }

int sizeof_char_from_int(char a) { return sizeof(a); }

_Bool bool_fn_add(_Bool x) { return x + 1; }
_Bool bool_fn_sub(_Bool x) { return x - 1; }

static int static_fn() { return 3; }

int param_decay(int x[]) { return x[0]; }

int main() {
  ASSERT(0, 0);
  ASSERT(42, 42);
  ASSERT(21, 5 + 20 - 4);
  ASSERT(41, 12 + 34 - 5);
  ASSERT(47, 5 + 6 * 7);
  ASSERT(8, 5 + 6 / 2);
  ASSERT(60, 3 * 4 * 5);
  ASSERT(2, 3 * 4 / 6);
  ASSERT(15, 5 * (9 - 6));
  ASSERT(4, (3 + 5) / 2);
  ASSERT(10, -10 + 20);
  ASSERT(15, -(-3 * +5));
  ASSERT(0, 0 == 1);
  ASSERT(1, 42 == 42);
  ASSERT(1, 0 != 1);
  ASSERT(0, 42 != 42);
  ASSERT(1, 0 < 1);
  ASSERT(0, 1 < 1);
  ASSERT(0, 2 < 1);
  ASSERT(1, 0 <= 1);
  ASSERT(1, 1 <= 1);
  ASSERT(0, 2 <= 1);
  ASSERT(1, 1 > 0);
  ASSERT(0, 1 > 1);
  ASSERT(0, 1 > 2);
  ASSERT(1, 1 >= 0);
  ASSERT(1, 1 >= 1);
  ASSERT(0, 1 >= 2);
  ASSERT(1, ({
           int a;
           a = 1;
           a;
         }));
  ASSERT(26, ({
           int z;
           z = 26;
           z;
         }));
  ASSERT(1, ({
           int a;
           a = 1;
         }));
  ASSERT(41, ({
           int z;
           z = 12 + 34 - 5;
         }));
  ASSERT(3, ({
           1;
           2;
           3;
         }));
  ASSERT(2, ({
           int foo;
           foo = 2;
           foo;
         }));
  ASSERT(5, ({
           int foo;
           foo = 2;
           int bar;
           bar = foo;
           bar + 3;
         }));
  ASSERT(8, ret(8));
  ASSERT(3, ({
           int returnx;
           returnx = 3;
         }));
  ASSERT(5, ({
           int a;
           a = 1;
           if (a) a = 5;
           a;
         }));
  ASSERT(4, ({
           int a;
           a = 0;
           if (a)
             a = 5;
           else
             a = 4;
           a;
         }));
  ASSERT(5, ({
           int i;
           i = 0;
           while (i < 5) i = i + 1;
           i;
         }));
  ASSERT(3, ({
           int i;
           i = 3;
           while (0) i = i + 1;
           i;
         }));
  ASSERT(5, ({
           int i;
           for (i = 0; i < 5; i = i + 1) i;
           i;
         }));
  ASSERT(4, ({
           { 2; }
           { 3; }
           4;
         }));
  ASSERT(1, ({
           int a;
           a = 0;
           int b;
           b = a;
           int c;
           c = b;
           a == c;
         }));
  ASSERT(5, ({
           int i;
           i = 0;
           while (i < 5) {
             i = i + 1;
           }
           i;
         }));
  ASSERT(13, add(6, 7));
  ASSERT(62, addmul(6, 7, 8));
  ASSERT(21, sum(1, 2, 3, 4, 5, 6));
  ASSERT(4, sub(7, 3));
  ASSERT(2, sub(4, 2));
  ASSERT(6, ave(1, 3, 5, 7, 9, 11));
  ASSERT(3, ({
           int x;
           x = 3;
           int* y;
           y = &x;
           *y;
         }));
  ASSERT(5, ({
           int x;
           x = 3;
           int* y;
           y = &x;
           *y = 5;
           x;
         }));
  ASSERT(3, ({
           int x;
           x = 3;
           int* y;
           y = &x;
           int** z;
           z = &y;
           **z;
         }));
  ASSERT(7, ({
           int x;
           x = 7;
           deref(&x);
         }));
  ASSERT(1, ({
           int* p;
           alloc(&p, 1, 2, 4, 8);
           *p;
         }));
  ASSERT(2, ({
           int* p;
           alloc(&p, 1, 2, 4, 8);
           *(p + 1);
         }));
  ASSERT(4, ({
           int* p;
           alloc(&p, 1, 2, 4, 8);
           *(p + 2);
         }));
  ASSERT(8, ({
           int* p;
           alloc(&p, 1, 2, 4, 8);
           *(p + 3);
         }));
  ASSERT(2, ({
           int* p;
           alloc(&p, 1, 2, 4, 8);
           *(p + 3 - 2);
         }));
  ASSERT(15, ({
           int* p;
           alloc(&p, 1, 2, 4, 8);
           *p + *(p + 1) + *(p + 2) + *(p + 3);
         }));
  ASSERT(4, sizeof 1);
  ASSERT(4, ({
           int a;
           sizeof(a);
         }));
  ASSERT(8, ({
           int a;
           sizeof(&a);
         }));
  ASSERT(40, ({
           int as[10];
           sizeof(as);
         }));
  ASSERT(8, ({
           int as[10];
           sizeof(&as);
         }));
  ASSERT(6, ({
           int a;
           a = 2;
           int b[10];
           int c;
           c = 4;
           a + c;
         }));
  ASSERT(1, ({
           int as[10];
           *as = 1;
           *as;
         }));
  ASSERT(2, ({
           int as[10];
           *as = 1;
           *(as + 1) = 2;
           *(as + 1);
         }));
  ASSERT(6, ({
           int as[10];
           *as = 1;
           *(as + 4) = 2;
           *(as + 9) = 3;
           *as + *(as + 4) + *(as + 9);
         }));
  ASSERT(4, ({
           int as[10];
           int* last;
           last = as + 9;
           *(last - 5) = 4;
           *(last - 5);
         }));
  ASSERT(1, ({
           int as[10];
           as[0] = 1;
           as[0];
         }));
  ASSERT(2, ({
           int as[10];
           as[0] = 1;
           as[1] = 2;
           as[1];
         }));
  ASSERT(6, ({
           int as[10];
           as[0] = 1;
           as[4] = 2;
           as[9] = 3;
           as[0] + as[4] + as[9];
         }));
  ASSERT(4, ({
           int as[10];
           int middle;
           middle = 9 - 5;
           as[middle] = 4;
           as[middle];
         }));
  ASSERT(55, ({
           int as[10];
           int i;
           for (i = 0; i < 10; i = i + 1) {
             as[i] = i + 1;
           }
           int sum;
           sum = 0;
           for (i = 0; i < 10; i = i + 1) {
             sum = sum + as[i];
           }
           sum;
         }));
  ASSERT(45, ({
           int vs[10];
           add_vals(10, vs);
           sum_vals(10, vs);
         }));
  ASSERT(0, gx);
  ASSERT(3, ({
           gx = 3;
           gx;
         }));
  ASSERT(7, ({
           gx = 3;
           gy = 4;
           gx + gy;
         }));
  ASSERT(0, ({
           gxs[0] = 0;
           gxs[1] = 1;
           gxs[2] = 2;
           gxs[3] = 3;
           gxs[0];
         }));
  ASSERT(1, ({
           gxs[0] = 0;
           gxs[1] = 1;
           gxs[2] = 2;
           gxs[3] = 3;
           gxs[1];
         }));
  ASSERT(2, ({
           gxs[0] = 0;
           gxs[1] = 1;
           gxs[2] = 2;
           gxs[3] = 3;
           gxs[2];
         }));
  ASSERT(3, ({
           gxs[0] = 0;
           gxs[1] = 1;
           gxs[2] = 2;
           gxs[3] = 3;
           gxs[3];
         }));
  ASSERT(4, sizeof(gx));
  ASSERT(16, sizeof(gxs));
  ASSERT(1, ({
           char x;
           x = 1;
           x;
         }));
  ASSERT(3, ({
           char cx;
           cx = 1;
           char cy;
           cy = 2;
           cx + cy;
         }));
  ASSERT(1, ({
           char xx;
           xx = 1;
           sizeof(xx);
         }));
  ASSERT(8, ({
           char x;
           x = 1;
           sizeof(&x);
         }));
  ASSERT(10, ({
           char cs[10];
           sizeof(cs);
         }));
  ASSERT(5, ({
           char yy[3];
           yy[0] = 1;
           int yyy;
           yyy = 4;
           yy[0] + yyy;
         }));
  ASSERT(0, ""[0]);
  ASSERT(1, sizeof(""));
  ASSERT(97, "abc"[0]);
  ASSERT(98, "abc"[1]);
  ASSERT(99, "abc"[2]);
  ASSERT(4, sizeof("abc"));
  ASSERT(6, ({
           6  //*
              //*2
               ;
         }));
  ASSERT(2, ({
           int x;
           x = 2;
           {
             int x;
             x = 3;
           }
           x;
         }));
  ASSERT(3, ({
           int x;
           x = 3;
           {
             int x;
             x = 4;
           }
           {
             int y;
             y = 5;
           }
           int y;
           y = x;
           y;
         }));
  ASSERT(7, ({
           int x = 7;
           x;
         }));
  ASSERT(10, ({
           int x;
           int y = x = 5;
           x + y;
         }));
  ASSERT(42, ({ gz; }));
  ASSERT(55, fibo(9));

  ASSERT(3, (1, 2, 3));
  ASSERT(5, ({
           int i = 2;
           int j = 3;
           i = 5, j = 6;
           i;
         }));
  ASSERT(6, ({
           int i = 2;
           int j = 3;
           i = 5, j = 6;
           j;
         }));

  ASSERT(4, ({
           struct {
             int a;
           } x;
           sizeof(x);
         }));
  ASSERT(8, ({
           struct {
             int a;
             int b;
           } x;
           sizeof(x);
         }));
  ASSERT(12, ({
           struct {
             int a[3];
           } x;
           sizeof(x);
         }));
  ASSERT(16, ({
           struct {
             int a;
           } x[4];
           sizeof(x);
         }));
  ASSERT(24, ({
           struct {
             int a[3];
           } x[2];
           sizeof(x);
         }));
  ASSERT(2, ({
           struct {
             char a;
             char b;
           } x;
           sizeof(x);
         }));
  ASSERT(8, ({
           struct {
             char a;
             int b;
           } x;
           sizeof(x);
         }));
  ASSERT(8, ({
           struct {
             int a;
             char b;
           } x;
           sizeof(x);
         }));
  ASSERT(0, ({
           struct {
           } x;
           sizeof(x);
         }));
  ASSERT(1, ({
           struct {
             int a;
             int b;
           } x;
           x.a = 1;
           x.b = 2;
           x.a;
         }));
  ASSERT(2, ({
           struct {
             int a;
             int b;
           } x;
           x.a = 1;
           x.b = 2;
           x.b;
         }));
  ASSERT(1, ({
           struct {
             char a;
             int b;
             char c;
           } x;
           x.a = 1;
           x.b = 2;
           x.c = 3;
           x.a;
         }));
  ASSERT(2, ({
           struct {
             char a;
             int b;
             char c;
           } x;
           x.b = 1;
           x.b = 2;
           x.c = 3;
           x.b;
         }));
  ASSERT(3, ({
           struct {
             char a;
             int b;
             char c;
           } x;
           x.a = 1;
           x.b = 2;
           x.c = 3;
           x.c;
         }));
  ASSERT(0, ({
           struct {
             char a;
             char b;
           } x[3];
           char* p = x;
           p[0] = 0;
           x[0].a;
         }));
  ASSERT(1, ({
           struct {
             char a;
             char b;
           } x[3];
           char* p = x;
           p[1] = 1;
           x[0].b;
         }));
  ASSERT(2, ({
           struct {
             char a;
             char b;
           } x[3];
           char* p = x;
           p[2] = 2;
           x[1].a;
         }));
  ASSERT(3, ({
           struct {
             char a;
             char b;
           } x[3];
           char* p = x;
           p[3] = 3;
           x[1].b;
         }));

  ASSERT(6, ({
           struct {
             char a[3];
             char b[5];
           } x;
           char* p = &x;
           x.a[0] = 6;
           p[0];
         }));
  ASSERT(7, ({
           struct {
             char a[3];
             char b[5];
           } x;
           char* p = &x;
           x.b[0] = 7;
           p[3];
         }));

  ASSERT(6, ({
           struct {
             struct {
               char b;
             } a;
           } x;
           x.a.b = 6;
           x.a.b;
         }));

  ASSERT(4, ({
           int a = 1, b = 3;
           a + b;
         }));
  ASSERT(6, ({ ggx + ggy + ggz; }));
  ASSERT(10, ({
           struct {
             int a, b;
           } x;
           x.a = 9, x.b = 1;
           x.a + x.b;
         }));

  ASSERT(1, ({
           int x[3];
           &x[1] - x;
         }));
  ASSERT(2, ({
           int x[3];
           &x[2] - x;
         }));
  ASSERT(1, ({
           char x[3];
           &x[1] - x;
         }));
  ASSERT(2, ({
           char x[3];
           &x[2] - x;
         }));
  ASSERT(1, ({
           int x;
           int y;
           &x - &y;
         }));

  ASSERT(1, ({
           int x = 0;
           int y = 0;
           char z = 0;
           char* a = &y;
           char* b = &z;
           a - b;
         }));
  ASSERT(7, ({
           int x = 0;
           char y = 0;
           int z = 0;
           char* a = &y;
           char* b = &z;
           a - b;
         }));

  ASSERT(8, ({
           struct t {
             int a;
             int b;
           } x;
           struct t y;
           sizeof(y);
         }));
  ASSERT(8, ({
           struct t {
             int a;
             int b;
           };
           struct t y;
           sizeof(y);
         }));
  ASSERT(2, ({
           struct t {
             char a[2];
           };
           {
             struct t {
               char a[4];
             };
           }
           struct t y;
           sizeof(y);
         }));
  ASSERT(3, ({
           struct t {
             int x;
           };
           int t = 1;
           struct t y;
           y.x = 2;
           t + y.x;
         }));

  ASSERT(3, ({
           struct t {
             char a;
           } x;
           struct t* y = &x;
           x.a = 3;
           y->a;
         }));
  ASSERT(3, ({
           struct t {
             char a;
           } x;
           struct t* y = &x;
           y->a = 3;
           x.a;
         }));

  ASSERT(8, ({
           union {
             int a;
             char b[6];
           } x;
           sizeof(x);
         }));
  ASSERT(4, ({
           union {
             int a;
             char b[3];
           } x;
           sizeof(x);
         }));
  ASSERT(3, ({
           union {
             int a;
             char b[4];
           } x;
           x.a = 515;
           x.b[0];
         }));
  ASSERT(2, ({
           union {
             int a;
             char b[4];
           } x;
           x.a = 515;
           x.b[1];
         }));
  ASSERT(0, ({
           union {
             int a;
             char b[4];
           } x;
           x.a = 515;
           x.b[2];
         }));
  ASSERT(0, ({
           union {
             int a;
             char b[4];
           } x;
           x.a = 515;
           x.b[3];
         }));

  ASSERT(3, ({
           struct {
             int a, b;
           } x, y;
           x.a = 3;
           y = x;
           y.a;
         }));
  ASSERT(7, ({
           struct t {
             int a, b;
           };
           struct t x;
           x.a = 7;
           struct t y;
           struct t* z = &y;
           *z = x;
           y.a;
         }));
  ASSERT(7, ({
           struct t {
             int a, b;
           };
           struct t x;
           x.a = 7;
           struct t y, *p = &x, *q = &y;
           *q = *p;
           y.a;
         }));
  ASSERT(5, ({
           struct t {
             char a, b;
           } x, y;
           x.a = 5;
           y = x;
           y.a;
         }));
  ASSERT(8, ({
           struct t {
             char a, b;
           } x, y;
           x.a = 5;
           x.b = 3;
           y = x;
           y.a + y.b;
         }));
  ASSERT(3, ({
           union {
             int a, b;
           } x, y;
           x.a = 3;
           y.a = 5;
           y = x;
           y.a;
         }));
  ASSERT(8, ({
           union {
             int a, b;
           } x, y;
           x.b = 4;
           y = x;
           y.a + y.a;
         }));
  ASSERT(3, ({
           union {
             struct {
               int a, b;
             } c;
           } x, y;
           x.c.b = 3;
           y.c.b = 5;
           y = x;
           y.c.b;
         }));

  ASSERT(8, ({
           long x;
           sizeof(x);
         }));
  ASSERT(5, ({
           long x = 2;
           long y = 3;
           x + y;
         }));
  ASSERT(16, ({
           struct {
             char a;
             long b;
           } x;
           sizeof(x);
         }));
  ASSERT(1, sub_long(7, 3, 3));
  ASSERT(5, ret_long(5));

  ASSERT(2, ({
           short x;
           sizeof(x);
         }));
  ASSERT(5, ({
           short x = 2;
           short y = 3;
           x + y;
         }));
  ASSERT(4, ({
           struct {
             char a;
             short b;
           } x;
           sizeof(x);
         }));
  ASSERT(1, sub_short(7, 3, 3));

  ASSERT(24, ({
           int x[2][3];
           sizeof(x);
         }));
  ASSERT(12, ({
           int x[2][3];
           sizeof(x[0]);
         }));
  ASSERT(0, ({
           int x[2][3];
           int* y = x;
           *y = 0;
           **x;
         }));
  ASSERT(0, ({
           int x[2][3];
           int* y = x;
           *y = 0;
           x[0][0];
         }));
  ASSERT(1, ({
           int x[2][3];
           int* y = x;
           *(y + 1) = 1;
           *(*x + 1);
         }));
  ASSERT(1, ({
           int x[2][3];
           int* y = x;
           *(y + 1) = 1;
           x[0][1];
         }));
  ASSERT(2, ({
           int x[2][3];
           int* y = x;
           *(y + 2) = 2;
           *(*x + 2);
         }));
  ASSERT(2, ({
           int x[2][3];
           int* y = x;
           *(y + 2) = 2;
           x[0][2];
         }));
  ASSERT(3, ({
           int x[2][3];
           int* y = x;
           *(y + 3) = 3;
           **(x + 1);
         }));
  ASSERT(3, ({
           int x[2][3];
           int* y = x;
           *(y + 3) = 3;
           x[1][0];
         }));
  ASSERT(4, ({
           int x[2][3];
           int* y = x;
           *(y + 4) = 4;
           *(*(x + 1) + 1);
         }));
  ASSERT(4, ({
           int x[2][3];
           int* y = x;
           *(y + 4) = 4;
           x[1][1];
         }));
  ASSERT(5, ({
           int x[2][3];
           int* y = x;
           *(y + 5) = 5;
           *(*(x + 1) + 2);
         }));
  ASSERT(5, ({
           int x[2][3];
           int* y = x;
           *(y + 5) = 5;
           x[1][2];
         }));
  ASSERT(123, ({
           int x[2][3][4];
           int** y = x;
           int* z = y;
           z[6] = 123;
           x[0][1][2];
         }));

  ASSERT(7, "\a"[0]);
  ASSERT(8, "\b"[0]);
  ASSERT(9, "\t"[0]);
  ASSERT(10, "\n"[0]);
  ASSERT(11, "\v"[0]);
  ASSERT(12, "\f"[0]);
  ASSERT(13, "\r"[0]);
  ASSERT(27, "\e"[0]);
  ASSERT(106, "\j"[0]);
  ASSERT(107, "\k"[0]);
  ASSERT(108, "\l"[0]);
  ASSERT(7, "\ax\ny"[0]);
  ASSERT(120, "\ax\ny"[1]);
  ASSERT(10, "\ax\ny"[2]);
  ASSERT(121, "\ax\ny"[3]);
  ASSERT(5, sizeof("\ax\ny"));

  ASSERT(0, "\0"[0]);
  ASSERT(16, "\20"[0]);
  ASSERT(65, "\101"[0]);
  ASSERT(104, "\1500"[0]);
  ASSERT(48, "\1500"[1]);

  ASSERT(0, "\x0"[0]);
  ASSERT(0, "\x00"[0]);
  ASSERT(119, "\x77"[0]);
  ASSERT(127, "\x7f"[0]);
  ASSERT(90, "\x5a"[0]);
  ASSERT(90, "\x5A"[0]);
  ASSERT(15, "\x00f"[0]);
  ASSERT(15, "\x00F"[0]);
  ASSERT(90, "\x00FZ"[1]);

  ASSERT(24, ({
           char* x[3];
           sizeof(x);
         }));
  ASSERT(8, ({
           char(*x)[3];
           sizeof(x);
         }));
  ASSERT(1, ({
           char(x);
           sizeof(x);
         }));
  ASSERT(3, ({
           char(x)[3];
           sizeof(x);
         }));
  ASSERT(12, ({
           char(x[3])[4];
           sizeof(x);
         }));
  ASSERT(4, ({
           char(x[3])[4];
           sizeof(x[0]);
         }));
  ASSERT(3, ({
           char* x[3];
           char y;
           x[0] = &y;
           y = 3;
           x[0][0];
         }));
  ASSERT(4, ({
           char x[3];
           char(*y)[3] = x;
           y[0][0] = 4;
           y[0][0];
         }));

  ASSERT(0, ({
           void* x;
           0;
         }));

  ASSERT(1, ({
           char x;
           sizeof(x);
         }));
  ASSERT(2, ({
           short int x;
           sizeof(x);
         }));
  ASSERT(2, ({
           short int x;
           sizeof(x);
         }));
  ASSERT(2, ({
           int short x;
           sizeof(x);
         }));
  ASSERT(4, ({
           int x;
           sizeof(x);
         }));
  ASSERT(8, ({
           long int x;
           sizeof(x);
         }));
  ASSERT(8, ({
           int long x;
           sizeof(x);
         }));
  ASSERT(8, ({
           long long x;
           sizeof(x);
         }));
  ASSERT(8, ({
           long long int x;
           sizeof(x);
         }));

  ASSERT(1, ({
           MyChar x;
           sizeof(x);
         }));
  ASSERT(5, ({
           MyChar x = 5;
           x;
         }));
  ASSERT(4, ({
           MyInt x;
           sizeof(x);
         }));
  ASSERT(3, ({
           MyInt x = 3;
           x;
         }));
  ASSERT(4, ({
           MyInt2 x;
           sizeof(x);
         }));
  ASSERT(16, ({
           MyInts x;
           sizeof(x);
         }));
  ASSERT(4, ({
           DefaultType x;
           sizeof(x);
         }));
  ASSERT(6, ({
           DefaultType x = 2, y = 4;
           x + y;
         }));
  ASSERT(1, ({
           typedef char;
           char x;
           sizeof x;
         }));
  ASSERT(1, ({
           typedef int t;
           t x = 1;
           x;
         }));
  ASSERT(1, ({
           typedef struct {
             int a;
           } t;
           t x;
           x.a = 1;
           x.a;
         }));
  ASSERT(1, ({
           typedef int t;
           ({
             t t = 1;
             t + 0;
           });
         }));
  ASSERT(2, ({
           typedef struct {
             int a;
           } t;
           { typedef int t; }
           t x;
           x.a = 2;
           x.a;
         }));

  ASSERT(1, sizeof(char));
  ASSERT(2, sizeof(short));
  ASSERT(2, sizeof(short int));
  ASSERT(2, sizeof(int short));
  ASSERT(4, sizeof(int));
  ASSERT(8, sizeof(long));
  ASSERT(8, sizeof(long int));
  ASSERT(8, sizeof(long int));
  ASSERT(8, sizeof(char*));
  ASSERT(8, sizeof(int*));
  ASSERT(8, sizeof(long*));
  ASSERT(8, sizeof(int**));
  ASSERT(8, sizeof(int(*)[4]));
  ASSERT(32, sizeof(int* [4]));
  ASSERT(16, sizeof(int[4]));
  ASSERT(48, sizeof(int[3][4]));
  ASSERT(8, sizeof(struct {
           int a;
           int b;
         }));

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
           *(int*)y;
         }));
  ASSERT(-1, ({
    (void)1;
    -1;
  }));

  ASSERT(0, 1073741824 * 100 / 100);
  ASSERT(8, sizeof(-10 + (long)5));
  ASSERT(8, sizeof(-10 - (long)5));
  ASSERT(8, sizeof(-10 * (long)5));
  ASSERT(8, sizeof(-10 / (long)5));
  ASSERT(8, sizeof((long)-10 + 5));
  ASSERT(8, sizeof((long)-10 - 5));
  ASSERT(8, sizeof((long)-10 * 5));
  ASSERT(8, sizeof((long)-10 / 5));
  ASSERT((long)-5, -10 + (long)5);
  ASSERT((long)-15, -10 - (long)5);
  ASSERT((long)-50, -10 * (long)5);
  ASSERT((long)-2, -10 / (long)5);
  ASSERT(1, -2 < (long)-1);
  ASSERT(1, -2 <= (long)-1);
  ASSERT(0, -2 > (long)-1);
  ASSERT(0, -2 >= (long)-1);
  ASSERT(1, (long)-2 < -1);
  ASSERT(1, (long)-2 <= -1);
  ASSERT(0, (long)-2 > -1);
  ASSERT(0, (long)-2 >= -1);
  ASSERT(0, 2147483647 + 2147483647 + 2);
  ASSERT((long)-1, ({
           long x;
           x = -1;
           x;
         }));
  ASSERT(1, ({
           char x[3];
           x[0] = 0;
           x[1] = 1;
           x[2] = 2;
           char* y = x + 1;
           y[0];
         }));
  ASSERT(0, ({
           char x[3];
           x[0] = 0;
           x[1] = 1;
           x[2] = 2;
           char* y = x + 1;
           y[-1];
         }));
  ASSERT(5, ({
           struct t {
             char a;
           } x, y;
           x.a = 5;
           y = x;
           y.a;
         }));

  ASSERT(5, int_to_char(261));

  ASSERT(-5, div_long(-10, 2));
  ASSERT(11, sum_int_to_char(261, 262));
  ASSERT(1, sizeof_char_from_int(261));

  ASSERT(0, ({
           _Bool x = 0;
           x;
         }));
  ASSERT(1, ({
           _Bool x = 1;
           x;
         }));
  ASSERT(1, ({
           _Bool x = 2;
           x;
         }));
  ASSERT(1, (_Bool)1);
  ASSERT(1, (_Bool)2);
  ASSERT(0, (_Bool)(char)256);
  ASSERT(1, bool_fn_add(3));
  ASSERT(0, bool_fn_sub(3));
  ASSERT(1, bool_fn_add(-3));
  ASSERT(0, bool_fn_sub(-3));
  ASSERT(1, bool_fn_add(0));
  ASSERT(1, bool_fn_sub(0));

  ASSERT(97, 'a');
  ASSERT(10, '\n');
  ASSERT(-128, '\x80');

  ASSERT(0, ({
           enum { zero, one, two };
           zero;
         }));
  ASSERT(1, ({
           enum { zero, one, two };
           one;
         }));
  ASSERT(2, ({
           enum { zero, one, two };
           two;
         }));
  ASSERT(5, ({
           enum { five = 5, six, seven };
           five;
         }));
  ASSERT(6, ({
           enum { five = 5, six, seven };
           six;
         }));
  ASSERT(0, ({
           enum { zero, five = 5, three = 3, four };
           zero;
         }));
  ASSERT(5, ({
           enum { zero, five = 5, three = 3, four };
           five;
         }));
  ASSERT(3, ({
           enum { zero, five = 5, three = 3, four };
           three;
         }));
  ASSERT(4, ({
           enum { zero, five = 5, three = 3, four };
           four;
         }));
  ASSERT(4, ({
           enum { zero, one, two } x;
           sizeof(x);
         }));
  ASSERT(4, ({
           enum t { zero, one, two };
           enum t y;
           sizeof(y);
         }));

  ASSERT(3, static_fn());

  ASSERT(55, ({
           int j = 0;
           for (int i = 0; i <= 10; i = i + 1) j = j + i;
           j;
         }));
  ASSERT(3, ({
           int i = 3;
           int j = 0;
           for (int i = 0; i <= 10; i = i + 1) j = j + i;
           i;
         }));

  ASSERT(7, ({
           int i = 2;
           i += 5;
           i;
         }));
  ASSERT(7, ({
           int i = 2;
           i += 5;
         }));
  ASSERT(3, ({
           int i = 5;
           i -= 2;
           i;
         }));
  ASSERT(3, ({
           int i = 5;
           i -= 2;
         }));
  ASSERT(6, ({
           int i = 3;
           i *= 2;
           i;
         }));
  ASSERT(6, ({
           int i = 3;
           i *= 2;
         }));
  ASSERT(3, ({
           int i = 6;
           i /= 2;
           i;
         }));
  ASSERT(3, ({
           int i = 6;
           i /= 2;
         }));

  ASSERT(3, ({
           int i = 2;
           ++i;
         }));
  ASSERT(2, ({
           int a[3];
           a[0] = 0;
           a[1] = 1;
           a[2] = 2;
           int* p = a + 1;
           ++*p;
         }));
  ASSERT(0, ({
           int a[3];
           a[0] = 0;
           a[1] = 1;
           a[2] = 2;
           int* p = a + 1;
           --*p;
         }));
  ASSERT(1, ({
           char i;
           sizeof(++i);
         }));

  ASSERT(2, ({
           int i = 2;
           i++;
         }));
  ASSERT(2, ({
           int i = 2;
           i--;
         }));
  ASSERT(3, ({
           int i = 2;
           i++;
           i;
         }));
  ASSERT(1, ({
           int i = 2;
           i--;
           i;
         }));
  ASSERT(1, ({
           int a[3];
           a[0] = 0;
           a[1] = 1;
           a[2] = 2;
           int* p = a + 1;
           *p++;
         }));
  ASSERT(1, ({
           int a[3];
           a[0] = 0;
           a[1] = 1;
           a[2] = 2;
           int* p = a + 1;
           *p--;
         }));

  ASSERT(0, ({
           int a[3];
           a[0] = 0;
           a[1] = 1;
           a[2] = 2;
           int* p = a + 1;
           (*p++)--;
           a[0];
         }));
  ASSERT(0, ({
           int a[3];
           a[0] = 0;
           a[1] = 1;
           a[2] = 2;
           int* p = a + 1;
           (*(p--))--;
           a[1];
         }));
  ASSERT(2, ({
           int a[3];
           a[0] = 0;
           a[1] = 1;
           a[2] = 2;
           int* p = a + 1;
           (*p)--;
           a[2];
         }));
  ASSERT(2, ({
           int a[3];
           a[0] = 0;
           a[1] = 1;
           a[2] = 2;
           int* p = a + 1;
           (*p)--;
           p++;
           *p;
         }));

  ASSERT(0, ({
           int a[3];
           a[0] = 0;
           a[1] = 1;
           a[2] = 2;
           int* p = a + 1;
           (*p++)--;
           a[0];
         }));
  ASSERT(0, ({
           int a[3];
           a[0] = 0;
           a[1] = 1;
           a[2] = 2;
           int* p = a + 1;
           (*p++)--;
           a[1];
         }));
  ASSERT(2, ({
           int a[3];
           a[0] = 0;
           a[1] = 1;
           a[2] = 2;
           int* p = a + 1;
           (*p++)--;
           a[2];
         }));
  ASSERT(2, ({
           int a[3];
           a[0] = 0;
           a[1] = 1;
           a[2] = 2;
           int* p = a + 1;
           (*p++)--;
           *p;
         }));
  ASSERT(1, ({
           char i;
           sizeof(i++);
         }));

  ASSERT(511, 0777);
  ASSERT(0, 0x0);
  ASSERT(10, 0xa);
  ASSERT(10, 0XA);
  ASSERT(48879, 0xbeef);
  ASSERT(48879, 0xBEEF);
  ASSERT(48879, 0XBEEF);
  ASSERT(0, 0b0);
  ASSERT(1, 0b1);
  ASSERT(47, 0b101111);
  ASSERT(47, 0B101111);

  ASSERT(0, !1);
  ASSERT(0, !2);
  ASSERT(1, !0);
  ASSERT(1, !(char)0);
  ASSERT(0, !(long)3);
  ASSERT(4, sizeof(!(char)0));
  ASSERT(4, sizeof(!(long)0));

  ASSERT(-1, ~0);
  ASSERT(0, ~-1);

  ASSERT(5, 17 % 6);
  ASSERT(5, ((long)17) % 6);
  ASSERT(2, ({
           int i = 10;
           i %= 4;
           i;
         }));
  ASSERT(2, ({
           long i = 10;
           i %= 4;
           i;
         }));

  ASSERT(0, 0 & 1);
  ASSERT(1, 3 & 1);
  ASSERT(3, 7 & 3);
  ASSERT(10, -1 & 10);
  ASSERT(1, 0 | 1);
  ASSERT(0b10011, 0b10000 | 0b00011);
  ASSERT(0, 0 ^ 0);
  ASSERT(0, 0b1111 ^ 0b1111);
  ASSERT(0b110100, 0b111000 ^ 0b001100);
  ASSERT(2, ({
           int i = 6;
           i &= 3;
           i;
         }));
  ASSERT(7, ({
           int i = 6;
           i |= 3;
           i;
         }));
  ASSERT(10, ({
           int i = 15;
           i ^= 5;
           i;
         }));

  ASSERT(1, 0 || 1);
  ASSERT(1, 0 || (2 - 2) || 5);
  ASSERT(0, 0 || 0);
  ASSERT(0, 0 || (2 - 2));

  ASSERT(0, 0 && 1);
  ASSERT(0, (2 - 2) && 5);
  ASSERT(1, 1 && 5);

  ASSERT(8, sizeof(int(*)[10]));
  ASSERT(8, sizeof(int(*)[][10]));

  ASSERT(3, ({
           int x[2];
           x[0] = 3;
           param_decay(x);
         }));

  ASSERT(8, ({
           struct foo* bar;
           sizeof(bar);
         }));
  ASSERT(4, ({
           struct T* foo;
           struct T {
             int x;
           };
           sizeof(struct T);
         }));
  ASSERT(1, ({
           struct T {
             struct T* next;
             int x;
           } a;
           struct T b;
           b.x = 1;
           a.next = &b;
           a.next->x;
         }));
  ASSERT(4, ({
           typedef struct T T;
           struct T {
             int x;
           };
           sizeof(T);
         }));
  ASSERT(8, ({
           union foo* bar;
           sizeof(bar);
         }));
  ASSERT(4, ({
           union T* foo;
           union T {
             int x;
           };
           sizeof(union T);
         }));
  ASSERT(1, ({
           union T {
             union T* next;
             int x;
           } a;
           union T b;
           b.x = 1;
           a.next = &b;
           a.next->x;
         }));
  ASSERT(4, ({
           typedef union T T;
           union T {
             int x;
           };
           sizeof(T);
         }));

  ASSERT(3, ({
           int i = 0;
           goto a;
         a:
           i++;
         b:
           i++;
         c:
           i++;
           i;
         }));
  ASSERT(2, ({
           int i = 0;
           goto e;
         d:
           i++;
         e:
           i++;
         f:
           i++;
           i;
         }));
  ASSERT(1, ({
           int i = 0;
           goto i;
         g:
           i++;
         h:
           i++;
         i:
           i++;
           i;
         }));

  ASSERT(1, ({
           typedef int foo;
           goto foo;
         foo:;
           1;
         }));

  ASSERT(3, ({
           int i = 0;
           for (; i < 10; i++) {
             if (i == 3) break;
           }
           i;
         }));
  ASSERT(4, ({
           int i = 0;
           while (1) {
             if (i++ == 3) break;
           }
           i;
         }));
  ASSERT(3, ({
           int i = 0;
           for (; i < 10; i++) {
             for (;;) break;
             if (i == 3) break;
           }
           i;
         }));
  ASSERT(4, ({
           int i = 0;
           while (1) {
             while (1) break;
             if (i++ == 3) break;
           }
           i;
         }));

  ASSERT(10, ({
           int i = 0;
           int j = 0;
           for (; i < 10; i++) {
             if (i > 5) continue;
             j++;
           }
           i;
         }));
  ASSERT(6, ({
           int i = 0;
           int j = 0;
           for (; i < 10; i++) {
             if (i > 5) continue;
             j++;
           }
           j;
         }));
  ASSERT(10, ({
           int i = 0;
           int j = 0;
           for (; !i;) {
             for (; j != 10; j++) continue;
             break;
           }
           j;
         }));
  ASSERT(11, ({
           int i = 0;
           int j = 0;
           while (i++ < 10) {
             if (i > 5) continue;
             j++;
           }
           i;
         }));
  ASSERT(5, ({
           int i = 0;
           int j = 0;
           while (i++ < 10) {
             if (i > 5) continue;
             j++;
           }
           j;
         }));
  ASSERT(11, ({
           int i = 0;
           int j = 0;
           while (!i) {
             while (j++ != 10) continue;
             break;
           }
           j;
         }));

  ASSERT(5, ({
           int i = 0;
           switch (0) {
             case 0:
               i = 5;
               break;
             case 1:
               i = 6;
               break;
             case 2:
               i = 7;
               break;
           }
           i;
         }));
  ASSERT(6, ({
           int i = 0;
           switch (1) {
             case 0:
               i = 5;
               break;
             case 1:
               i = 6;
               break;
             case 2:
               i = 7;
               break;
           }
           i;
         }));
  ASSERT(7, ({
           int i = 0;
           switch (2) {
             case 0:
               i = 5;
               break;
             case 1:
               i = 6;
               break;
             case 2:
               i = 7;
               break;
           }
           i;
         }));
  ASSERT(0, ({
           int i = 0;
           switch (3) {
             case 0:
               i = 5;
               break;
             case 1:
               i = 6;
               break;
             case 2:
               i = 7;
               break;
           }
           i;
         }));
  ASSERT(5, ({
           int i = 0;
           switch (0) {
             case 0:
               i = 5;
               break;
             default:
               i = 7;
           }
           i;
         }));
  ASSERT(7, ({
           int i = 0;
           switch (1) {
             case 0:
               i = 5;
               break;
             default:
               i = 7;
           }
           i;
         }));
  ASSERT(2, ({
           int i = 0;
           switch (1) {
             case 0:
               0;
             case 1:
               0;
             case 2:
               0;
               i = 2;
           }
           i;
         }));
  ASSERT(0, ({
           int i = 0;
           switch (3) {
             case 0:
               0;
             case 1:
               0;
             case 2:
               0;
               i = 2;
           }
           i;
         }));
  ASSERT(3, ({
           int i = 0;
           switch (-1) {
             case 0xffffffff:
               i = 3;
               break;
           }
           i;
         }));

  ASSERT(1, 1 << 0);
  ASSERT(8, 1 << 3);
  ASSERT(10, 5 << 1);
  ASSERT(2, 5 >> 1);
  ASSERT(-1, -1 >> 1);
  ASSERT(1, ({
           int i = 1;
           i <<= 0;
           i;
         }));
  ASSERT(8, ({
           int i = 1;
           i <<= 3;
           i;
         }));
  ASSERT(10, ({
           int i = 5;
           i <<= 1;
           i;
         }));
  ASSERT(2, ({
           int i = 5;
           i >>= 1;
           i;
         }));
  ASSERT(-1, -1);
  ASSERT(-1, ({
    int i = -1;
    i;
  }));
  ASSERT(-1, ({
    int i = -1;
    i >>= 1;
    i;
  }));

  ASSERT(2, 0 ? 1 : 2);
  ASSERT(1, 1 ? 1 : 2);
  ASSERT(-1, 0 ? -2 : -1);
  ASSERT(-2, 1 ? -2 : -1);
  ASSERT(4, sizeof(0 ? 1 : 2));
  ASSERT(8, sizeof(0 ? (long)1 : (long)2));
  ASSERT(-1, 0 ? (long)-2 : -1);
  ASSERT(-1, 0 ? -2 : (long)-1);
  ASSERT(-2, 1 ? (long)-2 : -1);
  ASSERT(-2, 1 ? -2 : (long)-1);
  1 ? -2 : (void)-1;

  ASSERT(10, ({
           enum { ten = 1 + 2 + 3 + 4 };
           ten;
         }));
  ASSERT(1, ({
           int i = 0;
           switch (3) {
             case 5 - 2 + 0 * 3:
               i++;
           }
           i;
         }));
  ASSERT(8, ({
           int x[1 + 1];
           sizeof(x);
         }));
  ASSERT(6, ({
           char x[8 - 2];
           sizeof(x);
         }));
  ASSERT(6, ({
           char x[2 * 3];
           sizeof(x);
         }));
  ASSERT(3, ({
           char x[12 / 4];
           sizeof(x);
         }));
  ASSERT(2, ({
           char x[12 % 10];
           sizeof(x);
         }));
  ASSERT(0b100, ({
           char x[0b110 & 0b101];
           sizeof(x);
         }));
  ASSERT(0b111, ({
           char x[0b110 | 0b101];
           sizeof(x);
         }));
  ASSERT(0b110, ({
           char x[0b111 ^ 0b001];
           sizeof(x);
         }));
  ASSERT(4, ({
           char x[1 << 2];
           sizeof(x);
         }));
  ASSERT(2, ({
           char x[4 >> 1];
           sizeof(x);
         }));
  ASSERT(2, ({
           char x[(1 == 1) + 1];
           sizeof(x);
         }));
  ASSERT(1, ({
           char x[(1 != 1) + 1];
           sizeof(x);
         }));
  ASSERT(1, ({
           char x[(1 < 1) + 1];
           sizeof(x);
         }));
  ASSERT(2, ({
           char x[(1 <= 1) + 1];
           sizeof(x);
         }));
  ASSERT(2, ({
           char x[1 ? 2 : 3];
           sizeof(x);
         }));
  ASSERT(3, ({
           char x[0 ? 2 : 3];
           sizeof(x);
         }));
  ASSERT(3, ({
           char x[(1, 3)];
           sizeof(x);
         }));
  ASSERT(2, ({
           char x[!0 + 1];
           sizeof(x);
         }));
  ASSERT(1, ({
           char x[!1 + 1];
           sizeof(x);
         }));
  ASSERT(2, ({
           char x[~-3];
           sizeof(x);
         }));
  ASSERT(2, ({
           char x[(5 || 6) + 1];
           sizeof(x);
         }));
  ASSERT(1, ({
           char x[(0 || 0) + 1];
           sizeof(x);
         }));
  ASSERT(2, ({
           char x[(1 && 1) + 1];
           sizeof(x);
         }));
  ASSERT(1, ({
           char x[(1 && 0) + 1];
           sizeof(x);
         }));
  ASSERT(3, ({
           char x[(int)3];
           sizeof(x);
         }));
  ASSERT(15, ({
           char x[(char)0xffffff0f];
           sizeof(x);
         }));
  ASSERT(0x10f, ({
           char x[(short)0xffff010f];
           sizeof(x);
         }));
  ASSERT(4, ({
           char x[(int)0xfffffffffff + 5];
           sizeof(x);
         }));
  ASSERT(3, ({
           char x[(int*)16 - (int*)4];
           sizeof(x);
         }));

  ASSERT(1, ({
           int x[3] = {1, 2, 3};
           x[0];
         }));
  ASSERT(2, ({
           int x[3] = {1, 2, 3};
           x[1];
         }));
  ASSERT(3, ({
           int x[3] = {1, 2, 3};
           x[2];
         }));
  ASSERT(3, ({
           int x[3] = {1, 2, 3};
           x[2];
         }));

  ASSERT(2, ({
           int x[2][3] = {{1, 2, 3}, {4, 5, 6}};
           x[0][1];
         }));
  ASSERT(4, ({
           int x[2][3] = {{1, 2, 3}, {4, 5, 6}};
           x[1][0];
         }));
  ASSERT(6, ({
           int x[2][3] = {{1, 2, 3}, {4, 5, 6}};
           x[1][2];
         }));

  ASSERT(0, ({
           int x[3] = {};
           x[0];
         }));
  ASSERT(0, ({
           int x[3] = {};
           x[1];
         }));
  ASSERT(0, ({
           int x[3] = {};
           x[2];
         }));
  ASSERT(2, ({
           int x[2][3] = {{1, 2}};
           x[0][1];
         }));
  ASSERT(0, ({
           int x[2][3] = {{1, 2}};
           x[1][0];
         }));
  ASSERT(0, ({
           int x[2][3] = {{1, 2}};
           x[1][2];
         }));

  ASSERT(3, ({
           int x[3] = {1, 2, 3, 4, 5};
           x[2];
         }));
  ASSERT(2, ({
           int x[2][3] = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
           x[0][1];
         }));

  ASSERT('a', ({
           char x[4] = "abc";
           x[0];
         }));
  ASSERT('c', ({
           char x[4] = "abc";
           x[2];
         }));
  ASSERT(0, ({
           char x[4] = "abc";
           x[3];
         }));
  ASSERT('a', ({
           char x[2][4] = {"abc", "def"};
           x[0][0];
         }));
  ASSERT(0, ({
           char x[2][4] = {"abc", "def"};
           x[0][3];
         }));
  ASSERT('d', ({
           char x[2][4] = {"abc", "def"};
           x[1][0];
         }));
  ASSERT('f', ({
           char x[2][4] = {"abc", "def"};
           x[1][2];
         }));

  ASSERT(4, ({
           int x[] = {1, 2, 3, 4};
           x[3];
         }));
  ASSERT(16, ({
           int x[] = {1, 2, 3, 4};
           sizeof(x);
         }));
  ASSERT(4, ({
           typedef char T[];
           T x = "x";
           T y = "foo";
           sizeof(y);
         }));
  ASSERT(4, ({
           char x[] = "foo";
           sizeof(x);
         }));
  ASSERT(4, ({
           typedef char T[];
           T x = "foo";
           T y = "x";
           sizeof(x);
         }));
  ASSERT(2, ({
           typedef char T[];
           T x = "foo";
           T y = "x";
           sizeof(y);
         }));
  ASSERT(2, ({
           typedef char T[];
           T x = "x";
           T y = "foo";
           sizeof(x);
         }));

  ASSERT(1, ({
           struct {
             int a;
             int b;
             int c;
           } x = {1, 2, 3};
           x.a;
         }));
  ASSERT(2, ({
           struct {
             int a;
             int b;
             int c;
           } x = {1, 2, 3};
           x.b;
         }));
  ASSERT(3, ({
           struct {
             int a;
             int b;
             int c;
           } x = {1, 2, 3};
           x.c;
         }));
  ASSERT(1, ({
           struct {
             int a;
             int b;
             int c;
           } x = {1};
           x.a;
         }));
  ASSERT(0, ({
           struct {
             int a;
             int b;
             int c;
           } x = {1};
           x.b;
         }));
  ASSERT(0, ({
           struct {
             int a;
             int b;
             int c;
           } x = {1};
           x.c;
         }));
  ASSERT(1, ({
           struct {
             int a;
             int b;
           } x[2] = {{1, 2}, {3, 4}};
           x[0].a;
         }));
  ASSERT(2, ({
           struct {
             int a;
             int b;
           } x[2] = {{1, 2}, {3, 4}};
           x[0].b;
         }));
  ASSERT(3, ({
           struct {
             int a;
             int b;
           } x[2] = {{1, 2}, {3, 4}};
           x[1].a;
         }));
  ASSERT(4, ({
           struct {
             int a;
             int b;
           } x[2] = {{1, 2}, {3, 4}};
           x[1].b;
         }));
  ASSERT(0, ({
           struct {
             int a;
             int b;
           } x[2] = {{1, 2}};
           x[1].b;
         }));
  ASSERT(0, ({
           struct {
             int a;
             int b;
           } x = {};
           x.a;
         }));
  ASSERT(0, ({
           struct {
             int a;
             int b;
           } x = {};
           x.b;
         }));
  ASSERT(5, ({
           typedef struct {
             int a, b, c, d, e, f;
           } T;
           T x = {1, 2, 3, 4, 5, 6};
           T y;
           y = x;
           y.e;
         }));
  ASSERT(2, ({
           typedef struct {
             int a, b;
           } T;
           T x = {1, 2};
           T y, z;
           z = y = x;
           z.b;
         }));

  ASSERT(1, ({
           typedef struct {
             int a, b;
           } T;
           T x = {1, 2};
           T y = x;
           y.a;
         }));

  ok();
}