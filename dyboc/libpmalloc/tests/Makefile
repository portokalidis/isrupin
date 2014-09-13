CFLAGS+=-Wall -Werror -fPIC -O0 -g
LIBS+=-lpmalloc
LDFLAGS+=-L../


TESTS=memalign malloc calloc realloc exhaust
#TESTS2=exhaust

all: $(TESTS) $(TESTS2)

$(TESTS): %: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

$(TESTS2): %: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

clean:
	rm -f *.o $(TESTS)
