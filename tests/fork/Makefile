CC=gcc
CFLAGS=-O2 -g -Wall

TARGETS=fork fork2 fork3

all: $(TARGETS)

$(TARGETS): %: %.o
#$(CC) $(LDFLAGS) -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(TARGETS) *.o
