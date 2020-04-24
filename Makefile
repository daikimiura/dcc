CFLAGS=-std=c11 -g
SRCS=$(filter-out tests.c, $(wildcard *.c))
OBJS=$(SRCS:.c=.o)

dcc: $(OBJS)
		$(CC) -o dcc $(OBJS) $(LDFLAGS)

$(OBJS): dcc.h

test: dcc
	./dcc tests.c > tmp.s
	gcc -o tmp tmp.s
	./tmp

clean:
	rm -f dcc *.o *~ tmp*

.PHONY: test clean