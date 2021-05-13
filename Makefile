CFLAGS=-std=c11 -g -static -Wall

SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

TEST_DIR=test
TEST_SRCS=$(wildcard $(TEST_DIR)/*_test.c)
TEST_ASMS=$(TEST_SRCS:.c=.s)
TESTS=$(TEST_SRCS:.c=)

cc: $(OBJS)
	$(CC) -o cc $^ $(LDFLAGS)

$(OBJS): cc.h

test/%: cc test/adapter.c test/%.c
	$(CC) -o - -E -P -C test/$*.c | ./cc - -o test/$*.s
	$(CC) -o $@ test/adapter.c test/$*.s

.PHONY: %_test
%_test: test/%_test
	./test/$*_test
	rm -f test/$*_test.s test/$*_test

.PHONY: test
test: $(TESTS)
	./test/cli_test.sh; echo;
	for i in $^; do echo $$i; ./$$i || exit 1; echo; done;
	make clean

.PHONY: clean
clean:
	rm -f cc *.o $(TEST_ASMS) $(TESTS)