#GLIB_CFLAGS=$(shell pkg-config --cflags glib-2.0)
#GLIB_LIBS=$(shell pkg-config --libs glib-2.0)
LIBS+=$(GLIB_LIBS)
#CFLAGS+=$(GLIB_CFLAGS) -Wall -Werror -fPIC -O3
CFLAGS+=$(GLIB_CFLAGS) -Wall -Werror -fPIC -O0 -g
LIBS+=-lpmalloc
LDFLAGS+=-L../


TESTS=memalign


all: $(TESTS)

$(TESTS): %: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f *.o $(TESTS)
