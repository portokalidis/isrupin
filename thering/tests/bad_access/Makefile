CC=gcc
CFLAGS=-O0 -g -Wall
ES=execstack

TARGETS=bad

all: $(TARGETS)

bad: bad.o
	$(CC) -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(TARGETS) *.o
