CXX=g++
CC=gcc

include makefile.pin

EXTRA_CFLAGS=-O3 -Wall
CFLAGS=$(EXTRA_CFLAGS)
PIN_CXXFLAGS+=$(EXTRA_CFLAGS)


all: null.so bad


null.so: null.o
	$(CXX) $(PIN_LDFLAGS) -o $@ $^ $(PIN_LIBS)

bad: bad.o
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(PIN_CXXFLAGS) -c $^ 

%.o: %.c
	$(CC) $(CFLAGS) -c $^

PHONY: clean

clean:
	rm -f *.so *.o bad
