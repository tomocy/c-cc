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

# default output file
echo "int main() {}" > "$TMP/out.c"
# .c -> .o
rm -f "$TMP/out.o"
(cd "$TMP"; $OLDPWD/$CC $TMP/out.c)
if [ -f "$TMP/out.o" ]; then
    passed "out.c -> out.o"
else
    failed "out.c -> out.o"
fi
# .c -> .s
rm -f "$TMP/out.s"
(cd "$TMP"; $OLDPWD/$CC -S "$TMP/out.c")
if [ -f "$TMP/out.s" ]; then
    passed "out.c -> out.s"
else
    failed "out.c -> out.s"
fi

# multiple input files
echo "int x;" > "$TMP/out1.c"
echo "int y;" > "$TMP/out2.c"
# .c -> .o
rm -f "$TMP/out1.o" "$TMP/out2.o"
(cd "$TMP"; $OLDPWD/$CC $TMP/out1.c $TMP/out2.c)
if [ -f "$TMP/out1.o" ] && [ -f "$TMP/out2.o" ]; then
    passed "out1.c -> out1.o, out2.c -> out2.o"
else
    failed "out1.c -> out1.o, out2.c -> out2.o"
fi
# .c -> .s
rm -f "$TMP/out1.s" "$TMP/out2.s"
(cd "$TMP"; $OLDPWD/$CC -S $TMP/out1.c $TMP/out2.c)
if [ -f "$TMP/out1.s" ] && [ -f "$TMP/out2.s" ]; then
    passed "out1.c -> out1.s, out2.c -> out2.s"
else
    failed "out1.c -> out1.s, out2.c -> out2.s"
fi

# no input files
if $CC 2>&1 | grep -q 'no input files'; then
  passed 'no input files'
else
  failed 'no input files'
fi

# -o and multiple input files
if $CC -o out.o out1.c out2.c 2>&1 | grep -q "cannot specify '-o' with multiple filenames"; then
  passed '-o and multiple input files'
else
  failed '-o and multiple input files'
fi

echo "OK"
