#!/bin/bash

set -eu

CC=$1
COMMIT=dbe3e0c43e549a1602286144d94b0666549b18e6

# shellcheck disable=SC1091
REPO=https://github.com/rui314/libpng.git source ./test/3rd/clone.sh

CC="$CC" ./configure

sed -i 's/^wl=.*/wl=-Wl,/; s/^pic_flag=.*/pic_flag=-fPIC/' libtool

make clean
make -j
make test -j

make clean
git reset --hard "$COMMIT"
