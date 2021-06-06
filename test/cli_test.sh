#!/bin/bash

CC=$1

TMP=$(mktemp -d /tmp/cc_test_XXXXXX)
trap 'rm -rf $TMP' INT TERM HUP EXIT
EMPTY=$TMP/empty.c
touch "$EMPTY"

passed() {
  echo "testing $1 ... passed"
}

failed() {
  echo "testing $1 ... failed"
  exit 1
}

# no input files
if $CC 2>&1 | grep -q 'no input files'; then
  passed 'no input files'
else
  failed 'no input files'
fi

# -o
OUTPUT=$TMP/out
rm -rf "$OUTPUT"
$CC -o "$OUTPUT" "$EMPTY"
if [ -f "$OUTPUT" ]; then
  passed -o
else
  failed -o
fi

# --help
if $CC --help 2>&1 | grep -q cc; then
  passed --help
else
  failed --help
fi

echo "OK"
