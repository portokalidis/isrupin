CC=gcc
CFLAGS=-O3 -g -Wall

TARGETS=fork fork2

all: $(TARGETS)

$(TARGETS): %: %.o
#$(CC) $(LDFLAGS) -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(TARGETS) *.o
