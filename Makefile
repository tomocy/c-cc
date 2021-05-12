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

.PHONY: test
test: $(TESTS)
	./test/cli_test.sh; echo;
	for i in $^; do echo $$i; ./$$i || exit 1; echo; done;
	make clean

.PHONY: clean
clean:
	rm -f cc $(TEST_ASMS) $(TESTS)

.PHONY: build-container
build-container: Dockerfile
	docker build -t c-cc .

.PHONY: run-container
run-container:
	docker run -it --rm -v $(PWD):/home/cc/cc --name c-cc c-cc /bin/bash