CC=gcc
CFLAGS=-O3 -g -Wall
LDFLAGS=-L../../../DieHard/src -L.
LIBS=-ldiehard

TARGETS=simple_alloc simple_alloc_nodh pause libmymalloc.so libmymalloc2.so

all: $(TARGETS)

simple_alloc: simple_alloc.o
	$(CC) $(LDFLAGS) -o $@ $< $(LIBS)

simple_alloc_nodh: simple_alloc.o
	$(CC) -o $@ $<

pause: pause.o
	$(CC) $(LDFLAGS) -o $@ $< $(LIBS)


libmymalloc.so: mymalloc.c
	$(CC) -shared -fPIC -DMYMALLOC="\"mymalloc\"" -o $@ $< -ldl

libmymalloc2.so: mymalloc.c
	$(CC) -shared -fPIC -DMYMALLOC="\"mymalloc2\"" -o $@ $< -ldl

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(TARGETS) *.o
