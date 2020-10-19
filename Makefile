CFLAGS=-std=c11 -g -static -Wall
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)
TEST_SRCS=$(wildcard test/*_test.c)
TMP_TEST_SRCS=$(addprefix test/, $(addprefix tmp_, $(notdir $(TEST_SRCS))))
TEST_ASMS=$(TMP_TEST_SRCS:.c=.s)
TEST_ADAPTER_SRCS=$(filter-out $(TEST_SRCS), $(wildcard test/*.c))
TEST_ADAPTER_OBJS=$(TEST_ADAPTER_SRCS:.c=.o)

cc: $(OBJS)
	$(CC) -o cc $^ $(LDFLAGS)

$(OBJS): cc.h

cc_test: $(TEST_ASMS) $(TEST_ADAPTER_OBJS)
	$(CC) -o cc_test $^

$(TEST_ASMS): cc $(TMP_TEST_SRCS)
	./cc $(filter-out cc, $?) > $@

$(TMP_TEST_SRCS): $(TEST_SRCS)
	$(CC) -o $@ -P -E $?

$(TEST_ADAPTER_OBJS): $(TEST_ADAPTER_SRCS)

.PHONY: test
test: cc_test
	./cc_test
	make clean

.PHONY: clean
clean: cc
	rm -f cc cc_test *~ *.o test/*.o tmp* test/tmp*