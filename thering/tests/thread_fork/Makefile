CC=gcc
CFLAGS=-O0 -g -Wall
LIBS=-lpthread

TARGETS=thread_fork

all: $(TARGETS)

$(TARGETS): %: %.o
	$(CC) $(LDFLAGS) -o $@ $< $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(TARGETS) *.o
