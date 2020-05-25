CFLAGS=-std=c11 -g
SRCS=$(filter-out tests.c tests-extern.c, $(wildcard *.c))
OBJS=$(SRCS:.c=.o)

dcc: $(OBJS)
	$(CC) -o dcc $(OBJS) $(LDFLAGS)

$(OBJS): dcc.h

dcc-gen2: dcc $(SRCS) dcc.h
	./self.sh

tests_extern.o: tests_extern
	gcc -xc -c -o tests_extern.o tests_extern

test: dcc tests_extern.o
	./dcc tests > tmp.s
	gcc -o tmp tmp.s tests_extern.o
	./tmp

test-gen2: dcc-gen2 tests_extern.o
	./dcc-gen2 tests > tmp.s
	gcc -o tmp tmp.s tests_extern.o
	./tmp

clean:
	rm -rf dcc dcc-gen* *.o *.out *~ tmp*

.PHONY: test clean