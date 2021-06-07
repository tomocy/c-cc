#!/bin/bash

CC=$1

TMP=$(mktemp -d /tmp/cc-test-XXXXXX)
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

# --help
if $CC --help 2>&1 | grep -q cc; then
  passed --help
else
  failed --help
fi

# -S
if echo "int main() {}" | $CC -S -o - - | grep -q "main:"; then
    passed -S
else
    failed -S
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

rm -f "$TMP/out.o" "$TMP/out.s"
echo "int main() {}" > "$TMP/out.c"
# default output object filename
(cd "$TMP"; $OLDPWD/$CC $TMP/out.c)
if [ -f "$TMP/out.o" ]; then
    passed "out.c -> out.o"
else
    failed "out.c -> out.o"
fi

# default output assembly filename
(cd "$TMP"; $OLDPWD/$CC -S "$TMP/out.c")
if [ -f "$TMP/out.s" ]; then
    passed "out.c -> out.s"
else
    failed "out.c -> out.s"
fi

echo "OK"
