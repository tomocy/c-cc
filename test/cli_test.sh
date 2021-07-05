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
echo 'int x;' > "$TMP/out1.c"
echo "#include \"$TMP/out1.c\"" > "$TMP/out2.c"
if $CC -E -o - "$TMP/out2.c" | grep -q 'int x;'; then
  passed 'preprosess (-E)'
else
  failed 'preprocess (-E)'
fi

# compile (-S)
if echo 'int main() {}' | $CC -S -o - -x c - | grep -q 'main:'; then
  passed 'compile (-S)'
else
  failed 'compile (-S)'
fi

# assemble (-c)
rm -rf "$TMP/out"
echo 'int main() { return 0; }' | $CC -c -o "$TMP/out" -x c -
if [ -f "$TMP/out" ]; then
  passed 'assemble (-o)'
else
  failed 'assemble (-o)'
fi

# link
# .c -> executable
echo 'int bar(); int main() { return bar(); }' > "$TMP/out1.c"
echo 'int bar() { return 0; }' > "$TMP/out2.c"
$CC -o "$TMP/out" "$TMP/out1.c" "$TMP/out2.c"
if "$TMP/out"; then
  passed 'link (.c -> executable)'
else
  failed 'link (.c -> executable)'
fi
# .s -> executable
echo 'int x(); int main() { return x(); }' | $CC -S -o "$TMP/out1.s" -x c -
echo 'int x() { return 0; }' | $CC -S -o "$TMP/out2.s" -x c -
$CC -o "$TMP/out" "$TMP/out1.s" "$TMP/out2.s"
if "$TMP/out"; then
  passed 'link (.s -> executable)'
else
  failed 'link (.s -> executable)'
fi
# .o -> executable
echo 'int x(); int main() { return x(); }' | $CC -c -o "$TMP/out1.o" -x c -
echo 'int x() { return 0; }' | $CC -c -o "$TMP/out2.o" -x c -
$CC -o "$TMP/out" "$TMP/out1.o" "$TMP/out2.o"
if "$TMP/out"; then
  passed 'link (.o -> executable)'
else
  failed 'link (.o -> executable)'
fi

# default output file
echo 'int main() {}' > "$TMP/out.c"
# .c -> .stdout (-E)
if $CC -E "$TMP/out.c" | grep -q 'int main() {}'; then
  passed 'default output file (-E) (.c -> stdout)'
else
  failed 'default output file (-E) (.c -> stdout)'
fi
# .c -> .s (-S)
rm -f "$TMP/out.s"
(
  cd "$TMP"
  "$OLDPWD/$CC" -S "$TMP/out.c"
)
if [ -f "$TMP/out.s" ]; then
  passed 'default output file (-S) (.c -> .s)'
else
  failed 'default output file (-S) (.c -> .s)'
fi
# .c -> .o (-c)
rm -f "$TMP/out.o"
(
  cd "$TMP"
  "$OLDPWD/$CC" -c "$TMP/out.c"
)
if [ -f "$TMP/out.o" ]; then
  passed 'default output file (-c) (.c -> .o)'
else
  failed 'default output file (-c) (.c -> .o)'
fi
# .c -> a.out
rm -f "$TMP/a.out"
(
  cd "$TMP"
  "$OLDPWD/$CC" "$TMP/out.c"
)
if [ -f "$TMP/a.out" ]; then
  passed 'default output file (.c -> a.out)'
else
  failed 'default output file (.c -> a.out)'
fi

# multiple input files
echo 'int x; int y;' > "$TMP/out1.c"
echo 'extern int x; int main() { return x; }' > "$TMP/out2.c"
# .c -> .stdout (-E)
if
  $CC -E "$TMP/out1.c" "$TMP/out2.c" | grep -q 'int y;'
  $CC -E "$TMP/out1.c" "$TMP/out2.c" | grep -q 'int main'
then
  passed 'multiple input files (-E) (.c -> stdout)'
else
  failed 'multiple input files (-E) (.c -> stdout)'
fi
# .c -> .s (-S)
rm -f "$TMP/out1.s" "$TMP/out2.s"
(
  cd "$TMP"
  "$OLDPWD/$CC" -S "$TMP/out1.c" "$TMP/out2.c"
)
if [ -f "$TMP/out1.s" ] && [ -f "$TMP/out2.s" ]; then
  passed 'multiple input files (-S) (.c -> .s)'
else
  failed 'multiple input files (-S) (.c -> .s)'
fi
# .c -> .o (-c)
rm -f "$TMP/out1.o" "$TMP/out2.o"
(
  cd "$TMP"
  "$OLDPWD/$CC" -c "$TMP/out1.c" "$TMP/out2.c"
)
if [ -f "$TMP/out1.o" ] && [ -f "$TMP/out2.o" ]; then
  passed 'multiple input files (-c) (.c -> .o)'
else
  failed 'multiple input files (-c) (.c -> .o)'
fi
# .c -> a.out
rm -f "$TMP/a.out"
(
  cd "$TMP"
  "$OLDPWD/$CC" "$TMP/out1.c" "$TMP/out2.c"
)
if [ -f "$TMP/a.out" ]; then
  passed 'multiple input files (.c -> a.out)'
else
  failed 'multiple input files (.c -> a.out)'
fi

# included path (-include)
# user defined file
echo "int x = 0;" > "$TMP/include.c"
if
  echo "int y = 0;" | $CC -S -o - -include "$TMP/include.c" -x c - | grep -q x:
  echo "int y = 0;" | $CC -S -o - -include "$TMP/include.c" -x c - | grep -q y:
then
  passed 'included path (-include) (user defined file)'
else
  failed 'included path (-include) (user defined file)'
fi
# system file
if echo "NULL" | $CC -E -o - -I include -include stdio.h -x c - | grep -q 0; then
  passed 'included path (-include) (system file)'
else
  failed 'included path (-include) (system file)'
fi

# include path (-I)
mkdir "$TMP/include1"
mkdir "$TMP/include2"
echo 'int x;' > "$TMP/include1/x.c"
echo 'int y;' > "$TMP/include2/y.c"
echo "#include \"x.c\"" > "$TMP/out.c"
echo "#include \"y.c\"" >> "$TMP/out.c"
if
  $CC -E -o - -I "$TMP/include1" -I "$TMP/include2" "$TMP/out.c" | grep -q 'int x;'
  $CC -E -o - -I "$TMP/include1" -I "$TMP/include2" "$TMP/out.c" | grep -q 'int y;'
then
  passed 'include path (-I)'
else
  failed 'include path (-I)'
fi

# include path (-idirafter)
rm -rf "$TMP/include1" "$TMP/include2"
mkdir "$TMP/include1"
mkdir "$TMP/include2"
echo 'int x = 0;' > "$TMP/include1/include.c"
echo 'int y = 0;' > "$TMP/include2/include.c"
if
  echo "#include \"include.c\"" | $CC -S -o - -I "$TMP/include1" -I "$TMP/include2" -x c - | grep -q y:
  echo "#include \"include.c\"" | $CC -S -o - -idirafter "$TMP/include1" -I "$TMP/include2" -x c - | grep -q x:
then
  passed 'include path (-idirafter)'
else
  failed 'include path (-idirafter)'
fi

# define macro (-D)
# boolean
if echo 'foo' | $CC -E -D foo -o - -x c - | grep -q '1'; then
  passed 'define macro (-D) (boolean)'
else
  failed 'define macro (-D) (boolean)'
fi
# with body
if echo 'foo' | $CC -E -D foo=bar -o - -x c - | grep -q 'bar'; then
  passed 'define macro (-D) (with body)'
else
  failed 'define macro (-D) (with body)'
fi
# once undefined
if echo 'foo' | $CC -E -D foo=bar -U foo -D foo -o - -x c - | grep -q '1'; then
  passed 'define macro (-D) (once undefined)'
else
  failed 'define macro (-D) (once undefined)'
fi

# undefine macro (-U)
if echo 'foo' | $CC -E -D foo -U foo -o - -x c - | grep -q 'foo'; then
  passed 'undefine macro (-U)'
else
  failed 'undefine macro (-U)'
fi

# enable common symbols
# default
if echo 'int x;' | $CC -S -o - -x c - | grep -q '.comm x'; then
  passed 'enable common symbols (default)'
else
  failed 'enable common symbols (default)'
fi
# -fcommon
if echo 'int x;' | $CC -S -o - -fcommon -x c - | grep -q '.comm x'; then
  passed 'enable common symbols (-fcommon)'
else
  failed 'enable common symbols (-fcommon)'
fi

#disable common symbols (-fno-common)
if
  echo 'int x;' | $CC -S -o - -fno-common -x c - | grep -q '.comm x' && exit 1
  echo 'int x;' | $CC -S -o - -fno-common -x c - | grep -q 'x:'
then
  passed 'disable common symbols (-fno-common)'
else
  failed 'disable common symbols (-fno-common)'
fi

# specify input language (-x)
# none
echo "int main() { return 0; }" > "$TMP/out.c"
$CC -o "$TMP/out" -x none "$TMP/out.c"
if "$TMP/out"; then
  passed 'specify input language (-x) (none)'
else
  failed 'specify input language (-x) (none)'
fi
# c
$CC -o "$TMP/out" -x c "$TMP/out.c"
if "$TMP/out"; then
  passed 'specify input language (-x) (c)'
else
  failed 'specify input language (-x) (c)'
fi
# assembler
$CC -S -o - "$TMP/out.c" | $CC -o "$TMP/out" -x assembler -
if "$TMP/out"; then
  passed 'specify input language (-x) (assembler)'
else
  failed 'specify input language (-x) (assembler)'
fi
# overridden when an input file is object file
echo "int main() { return 0; }" | $CC -c -o "$TMP/out.o" -x c -
$CC -o "$TMP/out" -x c "$TMP/out.o"
if "$TMP/out"; then
  passed 'specify input language (-x) (overridden when an input file is object file)'
else
  failed 'specify input language (-x) (overridden when an input file is object file)'
fi

# ignore options for now
if echo 'int x;' | $CC -E -o /dev/null -x c - \
  -O -W -g -std=c11 -ffreestanding -fno-builtin \
  -fno-omit-frame-pointer -fno-stack-protector -fno-strict-aliasing \
  -m64 -mno-red-zone -w; then
  passed 'ignore options for now'
else
  failed 'ignore options for now'
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

# validte multiple input files
# -o and -c
if $CC -c -o out.o out1.c out2.c 2>&1 | grep -q "cannot specify '-o' with '-c', '-S' or '-E' with multiple files"; then
  passed 'validate: multiple input files (-o and -c)'
else
  failed 'validate: multiple input files (-o and -c)'
fi
# -o and -S
if $CC -S -o out.s out1.c out2.c 2>&1 | grep -q "cannot specify '-o' with '-c', '-S' or '-E' with multiple files"; then
  passed 'validate: multiple input files (-o and -S)'
else
  failed 'validate: multiple input files (-o and -S)'
fi
# -o and -E
if $CC -E -o out.s out1.c out2.c 2>&1 | grep -q "cannot specify '-o' with '-c', '-S' or '-E' with multiple files"; then
  passed 'validate: multiple input files (-o and -E)'
else
  failed 'validate: multiple input files (-o and -E)'
fi

# Skip UTF-8 BOM
if echo -e '\xef\xbb\xbfxyz;' | $CC -E -o - -x c - | grep -q '^xyz'; then
  passed 'ignore UTF-8 BOM'
else
  failed 'ignore UTF-8 BOM'
fi

# inline function
# static implicitly
echo 'inline void foo() {}' > "$TMP/inline1.c"
echo 'inline void foo() {}' > "$TMP/inline2.c"
echo 'int main() { return 0; }' > "$TMP/out.c"
if $CC -o /dev/null "$TMP/inline1.c" "$TMP/inline2.c" "$TMP/out.c"; then
  passed 'inline function (static implicitly)'
else
  failed 'inline function (static implicitly)'
fi
# extern
echo 'extern inline void foo() {}' > "$TMP/inline1.c"
echo 'int foo(); int main() { foo(); }' > "$TMP/out.c"
if $CC -o /dev/null "$TMP/inline1.c" "$TMP/out.c"; then
  passed 'inline function (extern)'
else
  failed 'inline function (extern)'
fi
# static, not referred
if ! echo 'static inline void f1() {}' | $CC -S -o - -x c - | grep -q f1:; then
  passed 'inline function (static) (not referred)'
else
  failed 'inline function (static) (not referred)'
fi
# static, referred by external one
if echo 'static inline void f1() {} void f() { f1(); }' | $CC -S -o - -x c - | grep -q f1:; then
  passed 'inline function (static) (referred)'
else
  failed 'inline function (static) (referred)'
fi
# static, referred by not referred static one
if
  echo 'static inline void f1() {} static inline void f2() { f1(); }' | $CC -S -o - -x c - | grep -q f1: && exit 1
  ! echo 'static inline void f1() {} static inline void f2() { f1(); }' | $CC -S -o - -x c - | grep -q f2:
then
  passed 'inline function (static) (referred by not referred static one)'
else
  failed 'inline function (static) (referred by not referred static one)'
fi
# static, referred by each other, reffered by other
if
  echo 'static inline void f2(); static inline void f1() { f2(); } static inline void f2() { f1(); } void f() { f1(); }' | $CC -S -o - -x c - | grep -q f1:
  echo 'static inline void f2(); static inline void f1() { f2(); } static inline void f2() { f1(); } void f() { f1(); }' | $CC -S -o - -x c - | grep -q f2:
  echo 'static inline void f2(); static inline void f1() { f2(); } static inline void f2() { f1(); } void f() { f2(); }' | $CC -S -o - -x c - | grep -q f1:
  echo 'static inline void f2(); static inline void f1() { f2(); } static inline void f2() { f1(); } void f() { f2(); }' | $CC -S -o - -x c - | grep -q f2:
then
  passed 'inline function (static) (referred by each other) (referred by other)'
else
  failed 'inline function (static) (referred by each other) (referred by other)'
fi
# static, referred by each other, not reffered by other
if
  echo 'static inline void f2(); static inline void f1() { f2(); } static inline void f2() { f1(); }' | $CC -S -o - -x c - | grep -q f1: && exit 1
  ! echo 'static inline void f2(); static inline void f1() { f2(); } static inline void f2() { f1(); }' | $CC -S -o - -x c - | grep -q f2:
then
  passed 'inline function (static) (referred by each other) (not referred by other)'
else
  failed 'inline function (static) (referred by each other) (not referred by other)'
fi

echo 'OK'
