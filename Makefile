CFLAGS=-std=c11 -g
SRCS=$(filter-out tests.c, $(wildcard *.c))
OBJS=$(SRCS:.c=.o)

dcc: $(OBJS)
		$(CC) -o dcc $(OBJS) $(LDFLAGS)

$(OBJS): dcc.h

test: dcc
	./dcc tests.c > tmp.s
	echo 'int static_fn() {return 5;}' | gcc -xc -c -o tmp2.o -
	gcc -o tmp tmp.s tmp2.o
	./tmp

clean:
	rm -f dcc *.o *~ tmp*

.PHONY: test clean