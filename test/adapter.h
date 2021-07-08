#define ASSERT(x, y) assert(#y, x, y)

void assert(char* name, int expected, int actual);
void ok();

int strcmp(char* p, char* q);                  // NOLINT
int strncmp(char* p, char* q, long n);         // NOLINT
int memcmp(char* p, char* q, long n);          // NOLINT
void* memcpy(void* dest, void* src, long n);   // NOLINT
void* memset(void* s, int c, long n);          // NOLINT
long strlen(char* s);                          // NOLINT
int printf(char* fmt, ...);                    // NOLINT
int sprintf(char* buf, char* fmt, ...);        // NOLINT
int vsprintf(char* buf, char* fmt, void* ap);  // NOLINT
void exit(int n);