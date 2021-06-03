CFLAGS=-std=c11 -g -static -Wall

SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)
BETA_OBJS_ASMS=$(wildcard beta/*.o beta/*.s)
S2_ASMS=$(SRCS:%.c=s2/%.s)

TEST_DIR=test
TEST_SRCS=$(wildcard $(TEST_DIR)/*_test.c)
TEST_ASMS=$(TEST_SRCS:.c=.s)
TESTS=$(TEST_SRCS:.c=)

cc: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

test/%: cc test/adapter.c test/%.c
	$(CC) -o - -E -P -C test/$*.c | ./cc - -o test/$*.s
	$(CC) -o $@ test/adapter.c test/$*.s

beta/cc: $(BETA_OBJS_ASMS)
	$(CC) -o $@ $^ $(LDFLAGS)

beta/%.o: %.c
	rm -rf beta/$*.s
	$(CC) -c -o beta/$*.o $*.c

beta/%.s: cc beta/%.c
	rm -rf beta/$*.o
	./cc -o beta/$*.s beta/$*.c

beta/%.c: self.py %.c
	./self.py cc.h $*.c > beta/$*.c

s2/cc: $(S2_ASMS)
	$(CC) -o $@ $^ $(LDFLAGS)

s2/%.s: cc s2/%.c
	./cc -o s2/$*.s s2/$*.c

s2/%.c: self.py %.c
	./self.py cc.h $*.c > s2/$*.c

.PHONY: test
test: $(TESTS)
	./test/cli_test.sh; echo;
	for i in $^; do echo $$i; ./$$i || exit 1; echo; done;
	make clean

.PHONY: test/%
test@%: test/%_test
	./test/$*_test
	rm -f test/$*_test.s test/$*_test

.PHONY: beta/prepare
beta/prepare:
	make beta/main.o
	make beta/tokenize.o
	make beta/parse.o
	make beta/gen.o

.PHONY: beta/test@%
beta/test@%: s2/cc test/%_test.c
	make beta/prepare
	make beta/cc
	./beta/cc -o test/$*_test.s test/$*_test.c
	$(CC) -o test/$*_test test/$*_test.s
	./test/$*_test

.PHONY: s2/test@%
s2/test@%: s2/cc test/%_test.c
	./s2/cc -o test/$*_test.s test/$*_test.c
	$(CC) -o test/$*_test test/$*_test.s
	./test/$*_test

.PHONY: clean
clean:
	rm -f cc *.o $(TEST_ASMS) $(TESTS) beta/* s2/*