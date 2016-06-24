CC=gcc
CFLAGS=-O0 -g -Wall
ES=execstack

TARGETS=victim bad

all: $(TARGETS)

victim: victim.o
	$(CC) -o $@ $<
	$(ES) -s $@

bad: bad.o
	$(CC) -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(TARGETS) *.o
