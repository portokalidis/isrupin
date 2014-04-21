CXX=g++
CC=gcc

include makefile.pin

EXTRA_CFLAGS=-O3 -Wall
CFLAGS=$(EXTRA_CFLAGS)
PIN_CXXFLAGS+=$(EXTRA_CFLAGS)

LDFLAGS=-Wl,--hash-style=sysv -shared -Wl,-Bsymbolic -fPIC
CXXFLAGS=$(CFLAGS)

all: tricky.so spair.so


spair.so: spair.o
	$(CXX) $(LDFLAGS) -o $@ $^

spair.o: spair.c
	$(CXX) $(CXXFLAGS) -c $^ 

tricky.so: tricky.o
	$(CXX) $(PIN_LDFLAGS) -o $@ $^ $(PIN_LIBS)

tricky.o: tricky.cpp
	$(CXX) $(PIN_CXXFLAGS) -c $^ 

%.o: %.c
	$(CC) $(CFLAGS) -c $^

PHONY: clean

clean:
	rm -f *.so *.o bad
