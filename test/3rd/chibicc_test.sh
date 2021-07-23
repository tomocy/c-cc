#!/bin/bash

set -eu

CC=$1
COMMIT=90d1f7f199cc55b13c7fdb5839d1409806633fdb

# shellcheck disable=SC1091
DST=3rd REPO=https://github.com/rui314/chibicc.git source ./test/3rd/clone.sh

# Makefile
# shellcheck disable=SC2016
sed -e 's|\./chibicc|$(CHIBICC)|g' -i Makefile

# test/variable.c
sed -r '51s/.*/ASSERT(1, ({ int x; int y; char z; char \*a=\&y; char \*b=\&z; a-b; }));/' -i test/variable.c
sed -r '52s/.*/ASSERT(7, ({ int x; char y; int z; char \*a=\&y; char \*b=\&z; a-b; }));/' -i test/variable.c

# test/pointer.c
sed -r '6s/.*/ASSERT(3, ({ int x=3; int y=5; \*(\&y+1); }));/' -i test/pointer.c
sed -r '7s/.*/ASSERT(5, ({ int x=3; int y=5; \*(\&x-1); }));/' -i test/pointer.c
sed -r '8s/.*/ASSERT(3, ({ int x=3; int y=5; \*(\&y-(-1)); }));/' -i test/pointer.c
sed -r '10s/.*/ASSERT(7, ({ int x=3; int y=5; \*(\&y+1)=7; x; }));/' -i test/pointer.c
sed -r '11s/.*/ASSERT(7, ({ int x=3; int y=5; \*(\&x-2+1)=7; y; }));/' -i test/pointer.c

# test/alignof.c
sed -r '21s/.*/ASSERT(1, ({ _Alignas(char) char x, y; \&x-\&y; }));/' -i test/alignof.c
sed -r '22s/.*/ASSERT(8, ({ _Alignas(long) char x, y; \&x-\&y; }));/' -i test/alignof.c
sed -r '23s/.*/ASSERT(32, ({ _Alignas(32) char x, y; \&x-\&y; }));/' -i test/alignof.c
sed -r '24s/.*/ASSERT(32, ({ _Alignas(32) int \*x, \*y; ((char \*)\&x)-((char \*)\&y); }));/' -i test/alignof.c

# test/driver.sh
# shellcheck disable=SC2016
sed -r '24s/.*/$chibicc --help 2>\&1 | grep -q cc/' -i test/driver.sh
# shellcheck disable=SC2016
sed -r 's|\$OLDPWD/\$chibicc|$chibicc|g' -i test/driver.sh
# shellcheck disable=SC2016
sed -r '221s|.*|$chibicc -test/map|' -i test/driver.sh

make clean
make CHIBICC="$CC" test -j

make clean
git reset --hard "$COMMIT"
