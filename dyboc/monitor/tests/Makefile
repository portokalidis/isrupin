CFLAGS=-O0 -Wall -Werror
DFLAGS=-DMONITOR_DEBUG -DNDEBUG

TESTS=fork exec simple_under simple_over


.PHONY: all clean

all: $(TESTS)

$(TESTS): %: %.o
	$(CC) $(LDFLAGS) -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(TESTS) *.o
