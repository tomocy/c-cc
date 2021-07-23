#!/bin/bash

set -eu

CC=$1
COMMIT=86f477edaa17767b39c7bae5b67cac8580f7a8c1

# shellcheck disable=SC1091
REPO=https://github.com/sqlite/sqlite.git source ./test/3rd/clone.sh

CC="$CC" CFLAGS=-D_GNU_SOURCE ./configure

sed -i 's/^wl=.*/wl=-Wl,/; s/^pic_flag=.*/pic_flag=-fPIC/' libtool

sed -r 's/\{\[file writable .*\]\}/0/' -i test/attach.test
sed -r 's/\{\[file writable test.db\]\}/0/' -i test/temptable.test

make clean
make -j
make test -j

make clean
git reset --hard "$COMMIT"
