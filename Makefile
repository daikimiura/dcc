CFLAGS=-std=c11 -g

dcc: dcc.c

test: dcc
	./test.sh

clean:
	rm -f dcc *.o *~ tmp*

.PHONY: test clean