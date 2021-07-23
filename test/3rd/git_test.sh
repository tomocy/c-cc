#!/bin/bash

set -eu

CC=$1
COMMIT=54e85e7af1ac9e9a92888060d6811ae767fea1bc

# shellcheck disable=SC1091
REPO=https://github.com/git/git.git source ./test/3rd/clone.sh

make clean
make V=1 CC="$CC" test

make clean
git reset --hard "$COMMIT"
