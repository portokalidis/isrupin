CC=gcc
CFLAGS=-O3 -g -Wall

TARGETS=mmap_overlap munmap_partial mremap

all: $(TARGETS)

$(TARGETS): %: %.o
#$(CC) $(LDFLAGS) -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(TARGETS) *.o
