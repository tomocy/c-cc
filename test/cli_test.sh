#!/bin/bash

CC=$1

TMP=$(mktemp -d /tmp/cc-test-XXXXXX)
trap 'rm -rf $TMP' INT TERM HUP EXIT

passed() {
  echo "testing $1 ... passed"
}

failed() {
  echo "testing $1 ... failed"
  exit 1
}

# help (--help)
if $CC --help 2>&1 | grep -q cc; then
  passed 'help (--help)'
else
  failed 'help (--help)'
fi

# compile (-S)
if echo "int main() {}" | $CC -S -o - - | grep -q "main:"; then
    passed 'compile (-S)'
else
    failed 'compile (-S)'
fi

# assemble (-o)
rm -rf "$TMP/out"
echo "int main() { return 0; }" | $CC -c -o "$TMP/out" -
if [ -f "$TMP/out" ]; then
  passed 'assemble (-o)'
else
  failed 'assemble (-o)'
fi

# link
rm -rf "$TMP/out"
echo "int bar(); int main() { return bar(); }" > "$TMP/out1.c"
echo "int bar() { return 0; }" > "$TMP/out2.c"
$CC -o "$TMP/out" "$TMP/out1.c" "$TMP/out2.c"
if $TMP/out; then
    passed link
else
    failed link
fi

# default output file
echo "int main() {}" > "$TMP/out.c"
# .c -> .s
rm -f "$TMP/out.s"
(cd "$TMP"; $OLDPWD/$CC -S "$TMP/out.c")
if [ -f "$TMP/out.s" ]; then
    passed "default output file: out.c -> out.s"
else
    failed "default output file: out.c -> out.s"
fi
# .c -> .o
rm -f "$TMP/out.o"
(cd "$TMP"; $OLDPWD/$CC -c $TMP/out.c)
if [ -f "$TMP/out.o" ]; then
    passed "default output file: out.c -> out.o"
else
    failed "default output file: out.c -> out.o"
fi
# .c -> a.out
rm -f "$TMP/a.out"
(cd "$TMP"; $OLDPWD/$CC $TMP/out.c)
if [ -f "$TMP/a.out" ]; then
    passed "default output file: out.c -> a.out"
else
    failed "default output file: out.c -> a.out"
fi

# multiple input files
echo "int x;" > "$TMP/out1.c"
echo "extern int x; int main() { return x; }" > "$TMP/out2.c"
# .c -> .s
rm -f "$TMP/out1.s" "$TMP/out2.s"
(cd "$TMP"; $OLDPWD/$CC -S $TMP/out1.c $TMP/out2.c)
if [ -f "$TMP/out1.s" ] && [ -f "$TMP/out2.s" ]; then
    passed "multiple input files: out1.c -> out1.s, out2.c -> out2.s"
else
    failed "multiple input files: out1.c -> out1.s, out2.c -> out2.s"
fi
# .c -> .o
rm -f "$TMP/out1.o" "$TMP/out2.o"
(cd "$TMP"; $OLDPWD/$CC -c $TMP/out1.c $TMP/out2.c)
if [ -f "$TMP/out1.o" ] && [ -f "$TMP/out2.o" ]; then
    passed "multiple input files: out1.c -> out1.o, out2.c -> out2.o"
else
    failed "multiple input files: out1.c -> out1.o, out2.c -> out2.o"
fi
# .c -> a.out
rm -f "$TMP/a.out"
(cd "$TMP"; $OLDPWD/$CC $TMP/out1.c $TMP/out2.c)
if [ -f "$TMP/a.out" ]; then
    passed "multiple input files: out1.c, out2.c -> a.out"
else
    failed "multiple input files: out1.c, out2.c -> a.out"
fi

# validate no input files
if $CC 2>&1 | grep -q 'no input files'; then
  passed 'validate: no input files'
else
  failed 'validate: no input files'
fi

# validate multiple input files, -o and -c
if $CC -c -o out.o out1.c out2.c 2>&1 | grep -q "cannot specify '-o' with '-c' or '-S' with multiple files"; then
  passed 'validate: multiple input files with -o and -c'
else
  failed 'validate: multiple input files with -o and -c'
fi
# validate multiple input files, -o and -S
if $CC -S -o out.s out1.c out2.c 2>&1 | grep -q "cannot specify '-o' with '-c' or '-S' with multiple files"; then
  passed 'validate: multiple input files with -o and -S'
else
  failed 'validate: multiple input files with -o and -S'
fi

echo "OK"
