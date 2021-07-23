CFLAGS=-std=c11 -g -static -Wall

# Files to make compilers
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)
S2_OBJS=$(SRCS:%.c=s2/%.o)

# Files to run tests
TEST_DIR=test
TEST_SRCS=$(wildcard $(TEST_DIR)/*_test.c)
TESTS=$(TEST_SRCS:.c=)
S2_TESTS=$(TESTS:%=s2/%)

# stage1
cc: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test/%: cc test/adapter.c test/%.c
	./cc -c -I test -o test/$*.o test/$*.c
	$(CC) -pthread -o $@ test/adapter.c test/$*.o
	rm test/$*.o

# stage2
s2/cc: $(S2_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

s2/%.o: cc %.c
	./cc -c -o s2/$*.o $*.c

s2/test/%: cc s2/cc test/adapter.c test/%.c
	./s2/cc -c -I include -I test -o s2/test/$*.o test/$*.c
	./cc -pthread -o $@ test/adapter.c s2/test/$*.o
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
	@echo "test/cli_test"; ./test/cli_test.sh ./cc && echo;
	for i in $^; do echo $$i; ./$$i || exit 1; echo; done;
	make clean

.PHONY: s1/test@cli
s1/test@cli: cc test/cli_test.sh
	./test/cli_test.sh ./cc

.PHONY: s1/test@%
s1/test@%: test/%_test
	./test/$*_test
	rm -f test/$*_test

.PHONY: s1/test/3rd@%
s1/test/3rd@%: cc test/3rd/%_test.sh
	./test/3rd/$*_test.sh "$(CURDIR)/cc"

# stage1 test debug
.PHONY: s1/debug@%
s1/debug@%: test/%_test
	gdb ./test/$*_test

# stage2 test
.PHONY: s2/test
s2/test: $(S2_TESTS)
	@echo "test/cli_test"; ./test/cli_test.sh ./cc && echo;
	for i in $^; do echo $$i; ./$$i || exit 1; echo; done;
	make clean

.PHONY: s2/test@cli
s2/test@cli: s2/cc test/cli_test.sh
	./test/cli_test.sh ./s2/cc

.PHONY: s2/test@%
s2/test@%: s2/test/%_test
	./s2/test/$*_test
	rm -f s2/test/$*_test

.PHONY: s2/test/3rd@%
s2/test/3rd@%: s2/cc test/3rd/%_test.sh
	./test/3rd/$*_test.sh "$(CURDIR)/s2/cc"

# stage2 test debug
.PHONY: s2/debug@%
s2/debug@%: s2/test/%_test
	gdb ./s2/test/$*_test

.PHONY: clean
clean:
	rm -f \
		*.o *.out \
		cc $(TESTS) $(OBJS) $(wildcard test/*.o test/*.s) \
		s2/cc $(S2_TESTS) $(S2_OBJS) $(wildcard s2/*.c) $(wildcard s2/test/*.o s2/test/*.s s2/test/*.c)