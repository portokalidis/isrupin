#CFLAGS+=$(GLIB_CFLAGS) -Wall -Werror -fPIC -O3
CFLAGS+=-Wall -D_GNU_SOURCE -fPIC
#CFLAGS+=-O0 -g 
CFLAGS+=-O2 -Werror
#CFLAGS+=-DPMALLOC_DEBUG
#CFLAGS+=-DPMALLOC_WHITELIST
LIBS=-ldl -lc


TARGETS=libpmalloc.so libpmalloc_under.so
PMALLOC_COMMON_OBJS=wrappers.o whitelist.o
PMALLOC_COMMON_HEADS=pmalloc.h whitelist.h
	

all: $(TARGETS)


libpmalloc.so: pmalloc.o $(PMALLOC_COMMON_OBJS)
	$(CC) -shared -Wl,-soname,$@ -o $@ $^ $(LIBS)

libpmalloc_under.so: pmalloc_under.o $(PMALLOC_COMMON_OBJS)
	$(CC) -shared -Wl,-soname,$@ -o $@ $^ $(LIBS)

pmalloc_under.o: pmalloc.c $(PMALLOC_COMMON_HEADS)
	$(CC) $(CFLAGS) -DPMALLOC_UNDER -o $@ -c $<

pmalloc.o: pmalloc.c $(PMALLOC_COMMON_HEADS)
	$(CC) $(CFLAGS) -o $@ -c $<

$(PMALLOC_COMMON_OBJS): %.o: %.c $(PMALLOC_COMMON_HEADS)
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -f *.o $(TARGETS)
