CFLAGS+=-Wall -Werror -fPIC -O0 -g


TESTS=memalign malloc calloc realloc over#exhaust
TESTS2=exhaust

all: $(TESTS) $(TESTS2)

$(TESTS): %: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

$(TESTS2): %: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

clean:
	rm -f *.o $(TESTS) $(TESTS2)
