CXX=clang++
CXXFLAGS=-Wall -Werror -std=c++11 $(shell llvm-config --cxxflags)
#-D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -Wall -Werror -fpic


preassure.so: preassure.o
	$(CXX) -shared -o $@ $<

clean:
	rm -f *.o *.so
