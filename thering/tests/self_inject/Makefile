CC=gcc
CFLAGS=-O0 -g -Wall

TARGETS=sinject

all: $(TARGETS)

sinject: sinject.o
	$(CC) -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(TARGETS) *.o
