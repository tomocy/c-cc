#!/bin/bash

set -eu

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

# preprocess (-E)
echo "int x;" > "$TMP/out1.c"
echo "#include \"$TMP/out1.c\"" > "$TMP/out2.c"
if $CC -E -o - "$TMP/out2.c" | grep -q "int x;"; then
  passed 'preprosess (-E)'
else
  failed 'preprocess (-E)'
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
if "$TMP/out"; then
  passed link
else
  failed link
fi

# default output file
echo "int main() {}" > "$TMP/out.c"
# .c -> .c (-E)
if $CC -E "$TMP/out.c" | grep -q "int main() {}"; then
  passed "default output file (-E): out.c -> stdout"
else
  failed "default output file (-E): out.c -> stdout"
fi
# .c -> .s (-S)
rm -f "$TMP/out.s"
(
  cd "$TMP"
  "$OLDPWD/$CC" -S "$TMP/out.c"
)
if [ -f "$TMP/out.s" ]; then
  passed "default output file (-S): out.c -> out.s"
else
  failed "default output file (-S): out.c -> out.s"
fi
# .c -> .o (-c)
rm -f "$TMP/out.o"
(
  cd "$TMP"
  "$OLDPWD/$CC" -c "$TMP/out.c"
)
if [ -f "$TMP/out.o" ]; then
  passed "default output file (-c): out.c -> out.o"
else
  failed "default output file (-c): out.c -> out.o"
fi
# .c -> a.out
rm -f "$TMP/a.out"
(
  cd "$TMP"
  "$OLDPWD/$CC" "$TMP/out.c"
)
if [ -f "$TMP/a.out" ]; then
  passed "default output file: out.c -> a.out"
else
  failed "default output file: out.c -> a.out"
fi

# multiple input files
echo "int x; int y;" > "$TMP/out1.c"
echo "extern int x; int main() { return x; }" > "$TMP/out2.c"
# .c -> .c (-E)
if
  $CC -E "$TMP/out1.c" "$TMP/out2.c" | grep -q "int y;"
  $CC -E "$TMP/out1.c" "$TMP/out2.c" | grep -q "int main"
then
  passed "multiple input files (-E): out1.c, out2.c -> stdout"
else
  failed "multiple input files (-E): out1.c, out2.c -> stdout"
fi
# .c -> .s (-S)
rm -f "$TMP/out1.s" "$TMP/out2.s"
(
  cd "$TMP"
  "$OLDPWD/$CC" -S "$TMP/out1.c" "$TMP/out2.c"
)
if [ -f "$TMP/out1.s" ] && [ -f "$TMP/out2.s" ]; then
  passed "multiple input files (-S): out1.c -> out1.s, out2.c -> out2.s"
else
  failed "multiple input files (-S): out1.c -> out1.s, out2.c -> out2.s"
fi
# .c -> .o (-c)
rm -f "$TMP/out1.o" "$TMP/out2.o"
(
  cd "$TMP"
  "$OLDPWD/$CC" -c "$TMP/out1.c" "$TMP/out2.c"
)
if [ -f "$TMP/out1.o" ] && [ -f "$TMP/out2.o" ]; then
  passed "multiple input files (-c): out1.c -> out1.o, out2.c -> out2.o"
else
  failed "multiple input files (-c): out1.c -> out1.o, out2.c -> out2.o"
fi
# .c -> a.out
rm -f "$TMP/a.out"
(
  cd "$TMP"
  "$OLDPWD/$CC" "$TMP/out1.c" "$TMP/out2.c"
)
if [ -f "$TMP/a.out" ]; then
  passed "multiple input files: out1.c, out2.c -> a.out"
else
  failed "multiple input files: out1.c, out2.c -> a.out"
fi

# include path (-I)
mkdir "$TMP/include1"
mkdir "$TMP/include2"
echo "int x;" > "$TMP/include1/x.c"
echo "int y;" > "$TMP/include2/y.c"
echo "#include \"x.c\"" > "$TMP/out.c"
echo "#include \"y.c\"" >> "$TMP/out.c"
if
  $CC -E -o - -I "$TMP/include1" -I "$TMP/include2" "$TMP/out.c" | grep -q "int x;"
  $CC -E -o - -I "$TMP/include1" -I "$TMP/include2" "$TMP/out.c" | grep -q "int y;"
then
  passed "include path"
else
  failed "include path"
fi

# define macro (-D)
if echo "foo" | $CC -E -D foo -o - - | grep -q "1"; then
  passed "define boolean macro"
else
  failed "define boolean macro"
fi
if echo "foo" | $CC -E -D foo=bar -o - - | grep -q "bar"; then
  passed "define macro with body"
else
  failed "define macro with body"
fi
if echo "foo" | $CC -E -D foo=bar -U foo -D foo -o - - | grep -q "1"; then
  passed "define once undefined macro"
else
  failed "define once undefined macro"
fi

# undefine macro (-U)
if echo "foo" | $CC -E -D foo -U foo -o - - | grep -q "foo"; then
  passed "undefine macro"
else
  failed "undefine macro"
fi

# ignore options for now
if echo "int x;" | $CC -E -o /dev/null - \
  -O -W -g -std=c11 -ffreestanding -fno-builtin \
  -fno-omit-frame-pointer -fno-stack-protector -fno-strict-aliasing \
  -m64 -mno-red-zone -w; then
  passed "ignore options for now"
else
  failed "ignore options for now"
fi

# validate unknown argument
if $CC -unknown 2>&1 | grep -q 'unknown argument: -unknown'; then
  passed 'validate: unknown argument'
else
  failed 'validate: unknown argument'
fi

# validate no input files
if $CC 2>&1 | grep -q 'no input files'; then
  passed 'validate: no input files'
else
  failed 'validate: no input files'
fi

# validate multiple input files, -o and -c
if $CC -c -o out.o out1.c out2.c 2>&1 | grep -q "cannot specify '-o' with '-c', '-S' or '-E' with multiple files"; then
  passed 'validate: multiple input files with -o and -c'
else
  failed 'validate: multiple input files with -o and -c'
fi
# validate multiple input files, -o and -S
if $CC -S -o out.s out1.c out2.c 2>&1 | grep -q "cannot specify '-o' with '-c', '-S' or '-E' with multiple files"; then
  passed 'validate: multiple input files with -o and -S'
else
  failed 'validate: multiple input files with -o and -S'
fi
# validate multiple input files, -o and -E
if $CC -E -o out.s out1.c out2.c 2>&1 | grep -q "cannot specify '-o' with '-c', '-S' or '-E' with multiple files"; then
  passed 'validate: multiple input files with -o and -E'
else
  failed 'validate: multiple input files with -o and -E'
fi

# Skip UTF-8 BOM
if echo -e "\xef\xbb\xbfxyz;" | $CC -E -o - - | grep -q '^xyz'; then
  passed 'Ignore BOM'
else
  failed 'Ignore BOM'
fi

echo "OK"
