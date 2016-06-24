CC=gcc
CFLAGS=-O2 -g -Wall -fno-inline

TARGETS=rtn

all: $(TARGETS)

$(TARGETS): %: %.o
#$(CC) $(LDFLAGS) -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(TARGETS) *.o
