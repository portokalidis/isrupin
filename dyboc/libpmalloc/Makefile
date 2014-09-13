#CFLAGS+=$(GLIB_CFLAGS) -Wall -Werror -fPIC -O3
CFLAGS+=-Wall -Werror -D_GNU_SOURCE -fPIC
CFLAGS+=-O0 -g #-DPMALLOC_DEBUG
#CFLAGS+=-O3
#CFLAGS+=-DPMALLOC_WHITELIST
LIBS=-ldl -lc


TARGETS=libpmalloc.so
PMALLOC_COMMON_OBJS=wrappers.o whitelist.o
PMALLOC_COMMON_HEADS=pmalloc.h whitelist.h
	

all: $(TARGETS)


libpmalloc.so: pmalloc.o $(PMALLOC_COMMON_OBJS)
	$(CC) -shared -Wl,-soname,$@ -o $@ $^ $(LIBS)

%.o: %.c $(PMALLOC_COMMON_HEADS)
	$(CC) $(CFLAGS) -DPMALLOC_DEFAULT -o $@ -c $<


clean:
	rm -f *.o $(TARGETS)
