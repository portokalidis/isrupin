CFLAGS+=-Wall -Werror -fPIC -O0 -g
LIBS+=-lpmalloc
LDFLAGS+=-L../


TESTS=memalign malloc calloc realloc


all: $(TESTS)

$(TESTS): %: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f *.o $(TESTS)
