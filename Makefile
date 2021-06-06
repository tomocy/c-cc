CFLAGS=-std=c11 -g -static -Wall

SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)
BETA_OBJS_ASMS=$(wildcard beta/*.o beta/*.s)
S2_ASMS=$(SRCS:%.c=s2/%.s)

TEST_DIR=test
TEST_SRCS=$(wildcard $(TEST_DIR)/*_test.c)
TEST_ASMS=$(TEST_SRCS:.c=.s)
TESTS=$(TEST_SRCS:.c=)
BETA_TEST_ASMS_SRCS=$(wildcard beta/test/*.s beta/test/*.c)
BETA_TESTS=$(TESTS:%=beta/%)
S2_TESTS_ASMS_SRCS=$(wildcard s2/test/*.s s2/test/*.c)
S2_TESTS=$(TESTS:%=s2/%)

# stage1
cc: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test/%: cc test/adapter.c test/%.c
	$(CC) -o - -E -P -C test/$*.c | ./cc - -o test/$*.s
	$(CC) -o $@ test/adapter.c test/$*.s

# beta (stage1 ~> stage2)
beta/cc: $(BETA_OBJS_ASMS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compile c files with $(CC)
beta/%.o: %.c
	rm -rf beta/$*.s
	$(CC) -c -o beta/$*.o $*.c

# Compile c files with ./cc
beta/%.s: cc beta/%.c
	rm -rf beta/$*.o
	./cc -o beta/$*.s beta/$*.c

beta/%.c: self.py %.c
	./self.py cc.h $*.c > beta/$*.c

# stage2
s2/cc: $(S2_ASMS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

s2/%.s: cc s2/%.c
	./cc -o s2/$*.s s2/$*.c

s2/%.c: self.py %.c
	./self.py cc.h $*.c > s2/$*.c

s2/test/%: s2/cc test/adapter.c test/%.c
	$(CC) -o - -E -P -C test/$*.c | ./s2/cc - -o s2/test/$*.s
	$(CC) -o $@ test/adapter.c s2/test/$*.s

# stage1 and stage2 test
.PHONY: test
test:
	@echo "--- stage1"
	make s1/test
	@echo ""
	@echo "--- stage2"
	make s2/test

.PHONY: test@%
test@%:
	@echo "--- stage1"
	make s1/test@$*
	@echo ""
	@echo "--- stage2"
	make s2/test@$*

# stage1 test
.PHONY: s1/test
s1/test: $(TESTS)
	./test/cli_test.sh ./cc; echo;
	for i in $^; do echo $$i; ./$$i || exit 1; echo; done;
	make clean

.PHONY: s1/test@%
s1/test@%: test/%_test
	./test/$*_test
	rm -f test/$*_test.s test/$*_test

# beta test
# Use 'o' or 's' depends on which compiler you use
.PHONY: beta/prepare
beta/prepare:
	make beta/main.s
	make beta/tokenize.s
	make beta/parse.s
	make beta/gen.s

.PHONY: beta/test@%
beta/test@%: test/adapter.c test/%_test.c
	make beta/prepare
	make beta/cc
	$(CC) -o - -E -P -C test/$*_test.c > beta/test/$*_test.c
	./beta/cc -o beta/test/$*_test.s beta/test/$*_test.c
	$(CC) -o beta/test/$*_test test/adapter.c beta/test/$*_test.s
	./beta/test/$*_test
	rm -f beta/test/$*_test.s beta/test/$*_test.c beta/test/$*_test

# stage2 test
.PHONY: s2/test
s2/test: $(S2_TESTS)
	./test/cli_test.sh ./s2/cc; echo;
	for i in $^; do echo $$i; ./$$i || exit 1; echo; done;
	make clean

.PHONY: s2/test@%
s2/test@%: s2/test/%_test
	./s2/test/$*_test
	rm -f s2/test/$*_test.s s2/test/$*_test

.PHONY: clean
clean:
	rm -f \
		cc *.o $(TEST_ASMS) $(TESTS) \
		beta/cc $(BETA_OBJS_ASMS) $(BETA_TEST_ASMS_SRCS) $(BETA_TESTS) \
		s2/cc $(S2_ASMS) $(S2_TESTS_ASMS_SRCS) $(S2_TESTS)