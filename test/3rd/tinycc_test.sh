#!/bin/bash

set -eu

CC=$1
COMMIT=df67d8617b7d1d03a480a28f9f901848ffbfb7ec

# shellcheck disable=SC1091
REPO=https://github.com/TinyCC/tinycc.git source ./test/3rd/clone.sh

./configure --cc="$CC"
make clean
make -j
make CC=cc test -j

make clean
git reset --hard "$COMMIT"
