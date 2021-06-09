CFLAGS=-std=c11 -g -static -Wall

# Files to make compilers
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)
BETA_OBJS=$(wildcard beta/*.o beta/*.s)
S2_OBJS=$(SRCS:%.c=s2/%.o)

# Files to run tests
TEST_DIR=test
TEST_SRCS=$(wildcard $(TEST_DIR)/*_test.c)
TESTS=$(TEST_SRCS:.c=)
BETA_TESTS=$(TESTS:%=beta/%)
S2_TESTS=$(TESTS:%=s2/%)

# stage1
cc: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test/%: cc test/adapter.c test/%.c
	$(CC) -o - -E -P -C test/$*.c | ./cc -c -o test/$*.o -
	$(CC) -o $@ test/adapter.c test/$*.o
	rm test/$*.o

# beta (stage1 ~> stage2)
beta/cc: $(BETA_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compile c files with $(CC)
beta/%.o: %.c
	rm -rf beta/$*.s
	$(CC) -c -o beta/$*.o $*.c

# Compile c files with ./cc
beta/%.s: cc beta/%.c
	rm -rf beta/$*.o
	./cc -S -o beta/$*.s beta/$*.c

beta/%.c: self.py %.c
	./self.py cc.h $*.c > beta/$*.c

# stage2
s2/cc: $(S2_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

s2/%.o: cc s2/%.c
	./cc -c -o s2/$*.o s2/$*.c

s2/%.c: self.py %.c
	./self.py cc.h $*.c > s2/$*.c

s2/test/%: s2/cc test/adapter.c test/%.c
	$(CC) -o - -E -P -C test/$*.c | ./s2/cc -c -o s2/test/$*.o -
	$(CC) -o $@ test/adapter.c s2/test/$*.o
	rm -rf s2/test/$*.o

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

.PHONY: s1/test@cli
s1/test@cli: cc test/cli_test.sh
	./test/cli_test.sh ./cc

.PHONY: s1/test@%
s1/test@%: test/%_test
	./test/$*_test
	rm -f test/$*_test

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
	./beta/cc -c -o beta/test/$*_test.o beta/test/$*_test.c
	$(CC) -o beta/test/$*_test test/adapter.c beta/test/$*_test.o
	./beta/test/$*_test
	rm -f beta/test/$*_test.o beta/test/$*_test.c beta/test/$*_test

# stage2 test
.PHONY: s2/test
s2/test: $(S2_TESTS)
	./test/cli_test.sh ./s2/cc; echo;
	for i in $^; do echo $$i; ./$$i || exit 1; echo; done;
	make clean

.PHONY: s2/test@cli
s2/test@cli: s2/cc test/cli_test.sh
	./test/cli_test.sh ./s2/cc

.PHONY: s2/test@%
s2/test@%: s2/test/%_test
	./s2/test/$*_test
	rm -f s2/test/$*_test

.PHONY: clean
clean:
	rm -f \
		cc $(TESTS) $(OBJS) $(wildcard test/*.o test/*.s) \
		beta/cc $(BETA_TESTS) $(BETA_OBJS) $(wildcard beta/*.c) $(wildcard beta/test/*.o beta/test/*.s beta/test/*.c) \
		s2/cc $(S2_TESTS) $(S2_OBJS) $(wildcard s2/test/*.o s2/test/*.s s2/test/*.c)