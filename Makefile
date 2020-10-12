CFLAGS=-std=c11 -g -static -Wall
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

cc: $(OBJS)
	$(CC) -o cc $(OBJS) $(LDFLAGS)

$(OBJS): cc.h

.PHONY: test
test: cc
	./test.sh
	make clean

.PHONY: clean
clean: cc
	rm -f cc *.o *~ tmp*