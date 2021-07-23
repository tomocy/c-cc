#!/bin/bash

set -eu

CC=$1
COMMIT=c75330605d4795850ec74fdc4d69aa5d92f76c00

# shellcheck disable=SC1091
REPO=https://github.com/python/cpython.git source ./test/3rd/clone.sh

sed -i -e 4466,4487d configure.ac
sed -i -e '4465s/^/\n/' configure.ac
# shellcheck disable=SC2016
sed -i -e '4466s/^/"$ax_cv_c_float_words_bigendian" = "no"/' configure.ac
autoreconf

CC="$CC" ./configure
make clean
make -j
make test -j

make clean
git reset --hard "$COMMIT"
