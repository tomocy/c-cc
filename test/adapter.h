#define ASSERT(x, y) assert(#y, x, y)

void assert(char* name, int expected, int actual);
void ok();

int strcmp(char* p, char* q);
int memcmp(char* p, char* q, long n);
int sprintf();