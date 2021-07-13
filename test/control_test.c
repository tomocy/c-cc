#include "adapter.h"

/*
 * This is a block comment.
 */

static int increment(int* count) {
  return ++(*count);
}

static int is_le(int x, int basis) {
  return x <= basis;
}

int main() {
  ASSERT(3, ({
    int x;
    if (0)
      x = 2;
    else
      x = 3;
    x;
  }));
  ASSERT(3, ({
    int x;
    if (1 - 1)
      x = 2;
    else
      x = 3;
    x;
  }));
  ASSERT(2, ({
    int x;
    if (1)
      x = 2;
    else
      x = 3;
    x;
  }));
  ASSERT(2, ({
    int x;
    if (2 - 1)
      x = 2;
    else
      x = 3;
    x;
  }));

  ASSERT(55, ({
    int i = 0;
    int j = 0;
    for (i = 0; i <= 10; i = i + 1) {
      j = i + j;
    }
    j;
  }));

  ASSERT(10, ({
    int i = 0;
    while (i < 10)
      i = i + 1;
    i;
  }));

  ASSERT(3, ({
    // NOLINTNEXTLINE
    1;
    // NOLINTNEXTLINE
    { 2; }
    3;
  }));
  ASSERT(5, ({
    ;
    ;
    ;
    5;
  }));

  ASSERT(10, ({
    int i = 0;
    while (i < 10)
      i = i + 1;
    i;
  }));
  ASSERT(55, ({
    int i = 0;
    int j = 0;
    while (i <= 10) {
      j = i + j;
      i = i + 1;
    }
    j;
  }));

  // NOLINTNEXTLINE
  ASSERT(3, (1, 2, 3));
  ASSERT(5, ({
    // NOLINTNEXTLINE
    int i = 2, j = 3;
    (i = 5, j) = 6;
    i;
  }));
  ASSERT(6, ({
    // NOLINTNEXTLINE
    int i = 2, j = 3;
    (i = 5, j) = 6;
    j;
  }));

  ASSERT(55, ({
    int j = 0;
    // NOLINTNEXTLINE
    for (int i = 0; i <= 10; i = i + 1)
      // NOLINTNEXTLINE
      j = j + i;
    j;
  }));
  ASSERT(3, ({
    int i = 3;
    int j = 0;
    // NOLINTNEXTLINE
    for (int i = 0; i <= 10; i = i + 1)
      // NOLINTNEXTLINE
      j = j + i;
    i;
  }));

  ASSERT(1, 0 || 1);
  ASSERT(1, 0 || (2 - 2) || 5);
  ASSERT(0, 0 || 0);
  ASSERT(0, 0 || (2 - 2));

  ASSERT(0, 0 && 1);
  ASSERT(0, (2 - 2) && 5);
  ASSERT(1, 1 && 5);

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
      if (i == 3)
        break;
    }
    i;
  }));
  ASSERT(4, ({
    int i = 0;
    while (1) {
      if (i++ == 3)
        break;
    }
    i;
  }));
  ASSERT(3, ({
    int i = 0;
    for (; i < 10; i++) {
      // NOLINTNEXTLINE
      for (;;)
        // NOLINTNEXTLINE
        break;
      if (i == 3)
        break;
    }
    i;
  }));
  ASSERT(4, ({
    int i = 0;
    while (1) {
      while (1)
        break;
      if (i++ == 3)
        break;
    }
    i;
  }));

  ASSERT(10, ({
    int i = 0;
    int j = 0;
    for (; i < 10; i++) {
      if (i > 5)
        continue;
      j++;
    }
    i;
  }));
  ASSERT(6, ({
    int i = 0;
    int j = 0;
    for (; i < 10; i++) {
      if (i > 5)
        continue;
      j++;
    }
    j;
  }));
  ASSERT(10, ({
    int i = 0;
    int j = 0;
    for (; !i;) {
      // NOLINTNEXTLINE
      for (; j != 10; j++)
        // NOLINTNEXTLINE
        continue;
      break;
    }
    j;
  }));
  ASSERT(11, ({
    int i = 0;
    int j = 0;
    while (i++ < 10) {
      if (i > 5)
        continue;
      j++;
    }
    i;
  }));
  ASSERT(5, ({
    int i = 0;
    int j = 0;
    while (i++ < 10) {
      if (i > 5)
        continue;
      j++;
    }
    j;
  }));
  ASSERT(11, ({
    int i = 0;
    int j = 0;
    while (!i) {
      while (j++ != 10)
        continue;
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
        // NOLINTNEXTLINE
        0;
      case 1:
        // NOLINTNEXTLINE
        0;
      case 2:
        // NOLINTNEXTLINE
        0;
        i = 2;
    }
    i;
  }));
  ASSERT(0, ({
    int i = 0;
    switch (3) {
      case 0:
        // NOLINTNEXTLINE
        0;
      case 1:
        // NOLINTNEXTLINE
        0;
      case 2:
        // NOLINTNEXTLINE
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

  ASSERT(7, ({
    int i = 0;
    int j = 0;
    do {
      j++;
    } while (i++ < 6);
    j;
  }));
  ASSERT(4, ({
    // NOLINTNEXTLINE
    int i = 0;
    int j = 0;
    int k = 0;
    do {
      if (++j > 3)
        break;
      continue;
      k++;
    } while (1);
    j;
  }));

  ASSERT(0, 0.0 && 0.0);
  ASSERT(0, 0.0 && 0.1);  // NOLINT
  ASSERT(0, 0.3 && 0.0);  // NOLINT
  ASSERT(1, 0.3 && 0.5);  // NOLINT
  ASSERT(0, 0.0 || 0.0);
  ASSERT(1, 0.0 || 0.1);  // NOLINT
  ASSERT(1, 0.3 || 0.0);  // NOLINT
  ASSERT(1, 0.3 || 0.5);  // NOLINT
  ASSERT(5, ({
    int x;
    if (0.0)
      x = 3;
    else
      x = 5;
    x;
  }));
  ASSERT(3, ({
    int x;
    if (0.1)  // NOLINT
      x = 3;
    else
      x = 5;
    x;
  }));
  ASSERT(5, ({
    int x = 5;
    if (0.0)
      x = 3;
    x;
  }));
  ASSERT(3, ({
    int x = 5;
    if (0.1)  // NOLINT
      x = 3;
    x;
  }));
  ASSERT(10, ({
    double i = 10.0;
    int j = 0;
    for (; i; i--, j++) {
      ;
    }
    j;
  }));
  ASSERT(10, ({
    double i = 10.0;
    int j = 0;
    do {
      j++;
    } while (--i);
    j;
  }));

  ASSERT(1, ({
    char* a = "--";
    int x = 0;
    if (a[0] == '-' && a[1] != '\0') {
      x = 1;
    }
    x;
  }));

  ASSERT(1, ({
    char* a = "(@m@)";
    int x = 0;
    if (strcmp(a, "(@m@)") == 0) {
      x = 1;
    }
    x;
  }));

  ASSERT(11, ({
    int a = 1;
    int b = 1;
    int c = 0;
    int d = 0;

    int x = 0;

    if (a || b) {
      x += 1;
    }
    if (a && b) {
      x += 2;
    }
    if (a || c) {
      x += 3;
    }
    if (a && c) {
      x += 4;
    }
    if (d || b) {
      x += 5;
    }
    if (d && b) {
      x += 6;
    }
    if (c || d) {
      x += 7;
    }
    if (c && d) {
      x += 8;
    }
    x;
  }));
  ASSERT(1, ({
    int a = 1;
    int b = 0;
    int c = 0;
    int d = 0;
    int e = 1;
    int f = 0;
    int g = 0;
    int h = 0;

    int x = 0;

    if (a || b || c || d || e || f || g || h) {
      x = 1;
    }
    x;
  }));

  ASSERT(1, ({
    int count = 0;
    int x = 0;
    if (is_le(increment(&count), 2) || is_le(increment(&count), 2)) {
      x = 1;
    }
    x;
  }));
  ASSERT(1, ({
    int count = 0;
    int x = 0;
    if (is_le(increment(&count), 1) && is_le(increment(&count), 2)) {
      x = 1;
    }
    x;
  }));

  ASSERT(2, ({
    int i = 0;
    switch (7) {
      case 0 ... 5:
        i = 1;
        break;
      case 6 ... 20:
        i = 2;
        break;
    }
    i;
  }));
  ASSERT(1, ({
    int i = 0;
    switch (7) {
      case 0 ... 7:
        i = 1;
        break;
      case 8 ... 10:
        i = 2;
        break;
    }
    i;
  }));
  ASSERT(1, ({
    int i = 0;
    switch (7) {
      case 0:
        i = 1;
        break;
      case 7 ... 7:
        i = 1;
        break;
    }
    i;
  }));

  ASSERT(3, ({
    void* p = &&v11;
    int i = 0;
    goto* p;
  v11:
    i++;
  v12:  // NOLINT
    i++;
  v13:  // NOLINT
    i++;
    i;
  }));
  ASSERT(2, ({
    void* p = &&v22;
    int i = 0;
    goto* p;
  v21:  // NOLINT
    i++;
  v22:
    i++;
  v23:  // NOLINT
    i++;
    i;
  }));
  ASSERT(1, ({
    void* p = &&v33;
    int i = 0;
    goto* p;
  v31:  // NOLINT
    i++;
  v32:  // NOLINT
    i++;
  v33:
    i++;
    i;
  }));

  ASSERT(3, ({
    static void* p[] = {&&v41, &&v42, &&v43};
    int i = 0;
    goto* p[0];
  v41:
    i++;
  v42:
    i++;
  v43:
    i++;
    i;
  }));
  ASSERT(2, ({
    static void* p[] = {&&v52, &&v52, &&v53};
    int i = 0;
    goto* p[1];
  v51:
    i++;
  v52:
    i++;
  v53:
    i++;
    i;
  }));
  ASSERT(1, ({
    static void* p[] = {&&v62, &&v62, &&v63};
    int i = 0;
    goto* p[2];
  v61:
    i++;
  v62:
    i++;
  v63:
    i++;
    i;
  }));

  ASSERT(20, ({
    int i = 0;
    do {
      for (int j = 0; j < 10; j++) {
        i++;
      }
    } while (i < 15);
    i;
  }));

  ASSERT(10, ({
    int x = 0;

    switch (0) {
      case 0:
      begin12345:
        if (x < 10) {
          if (x % 3 == 0) {
            x += 4;
          } else {
            x += 3;
          }
          goto begin12345;
        } else if (x % 10 == 0)
          break;
        else
          goto begin12345;
    }

  end12345:

    x;
  }));
  ASSERT(10, ({
    int x = 0;

    while (x < 100) {
      if (x < 10) {
        if (x % 3 == 0) {
          x += 4;
        } else {
          x += 3;
        }
        continue;
      } else if (x % 10 == 0)  // NOLINT
        break;
      else if (x % 5 == 0) {
        x++;
        continue;
      }  // NOLINT
      else
        continue;
    }

    x;
  }));

  ok();
}