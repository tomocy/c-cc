CFLAGS=-std=c11 -g -static -Wall

cc: cc.c

.PHONY: test
test: cc
	./test.sh

.PHONY: clean
clean: cc
	rm -f cc *.o *~ tmp*