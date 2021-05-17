#define ASSERT(x, y) assert(#y, x, y)

void assert(char* name, int expected, int actual);
void ok();

// NOLINTNEXTLINE
int strcmp(char* p, char* q);
// NOLINTNEXTLINE
int memcmp(char* p, char* q, long n);
// NOLINTNEXTLINE
int printf(char* fmt, ...);
// NOLINTNEXTLINE
int sprintf(char* buf, char* fmt, ...);