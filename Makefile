CFLAGS=-std=c11 -g -static -Wall

SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

TEST_DIR=test
TMP_TEST_PREFIX=tmp_
TEST_SRCS=$(filter-out $(wildcard $(TEST_DIR)/$(TMP_TEST_PREFIX)*.c), $(wildcard $(TEST_DIR)/*_test.c))
TMP_TEST_SRCS=$(subst $(TEST_DIR)/,$(TEST_DIR)/$(TMP_TEST_PREFIX),$(TEST_SRCS))
TEST_ASMS=$(TMP_TEST_SRCS:.c=.s)
TEST_ADAPTER_SRCS=$(filter-out $(TMP_TEST_SRCS), $(filter-out $(TEST_SRCS), $(wildcard $(TEST_DIR)/*.c)))
TEST_ADAPTER_OBJS=$(TEST_ADAPTER_SRCS:.c=.o)

cc: $(OBJS)
	$(CC) -o cc $^ $(LDFLAGS)

$(OBJS): cc.h

cc_test: $(TEST_ASMS) $(TEST_ADAPTER_OBJS)
	$(CC) -o cc_test $^

$(TEST_ASMS): cc $(TMP_TEST_SRCS)
	./cc $(@:.s=.c) > $@

$(TMP_TEST_SRCS): $(TEST_SRCS)
	$(CC) -o $@ -P -E $(subst $(TMP_TEST_PREFIX),,$@)

$(TEST_ADAPTER_OBJS): $(TEST_ADAPTER_SRCS)

.PHONY: test
test: cc_test
	./cc_test
	make clean

.PHONY: clean
clean: cc
	rm -f cc cc_test *~ *.o test/*.o tmp* test/tmp*

.PHONY: build-docker-container
build-docker-container: Dockerfile
	docker build -t c-cc .

.PHONY: run-docker-container
run-docker-container:
	docker run -it --rm -v $(PWD):/home/cc/cc --name c-cc c-cc /bin/bash