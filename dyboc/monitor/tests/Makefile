CFLAGS=-O0 -Wall -Werror
DFLAGS=-DMONITOR_DEBUG -DNDEBUG

#TESTS=fork exec 
TEST_OVERFLOW=simple_over fork_over
TEST_UNDERFLOW=simple_under fork_under


.PHONY: all clean

all: $(TEST_UNDERFLOW) $(TEST_OVERFLOW)

$(TEST_UNDERFLOW): %: %.o underflow.o
	$(CC) $(LDFLAGS) -o $@ $^

$(TEST_OVERFLOW): %: %.o overflow.o
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(TESTS) *.o
