#GLIB_CFLAGS=$(shell pkg-config --cflags glib-2.0)
#GLIB_LIBS=$(shell pkg-config --libs glib-2.0)
LIBS+=$(GLIB_LIBS)
CFLAGS+=$(GLIB_CFLAGS) -g -Wall -Werror
CFLAGS+=-fPIC

PROG_NAME=test_pmalloc
#LIBRARIES=libpmalloc.a libpmalloc_under.a libpmalloc_over.a
DLIBRARIES=libpmalloc.so libpmalloc_under.so libpmalloc_over.so
#CC = gcc
OBJS = main.o pmalloc.o


all: $(PROG_NAME) $(LIBRARIES) $(DLIBRARIES)

$(DLIBRARIES): %.so: %.o
	$(CC) -shared -Wl,-soname,$@ -o $@ $< -lc

$(LIBRARIES): %.a: %.o
	ar -rc $@ $<
	ranlib $@

libpmalloc.o: pmalloc.c pmalloc.h
	$(CC) $(CFLAGS) -DPMALLOC_DEFAULT -o $@ -c $<

libpmalloc_under.o: pmalloc.c pmalloc.h
	$(CC) $(CFLAGS) -DPMALLOC_UNDER -o $@ -c $<

libpmalloc_over.o: pmalloc.c pmalloc.h
	$(CC) $(CFLAGS) -DPMALLOC_OVER -o $@ -c $<

$(PROG_NAME): main.o libpmalloc.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

main.o: main.c pmalloc.h
	$(CC) $(CFLAGS) -c $< 

clean:
	rm -f *.o
	rm -f $(PROG_NAME) $(LIBRARIES) $(DLIBRARIES)
