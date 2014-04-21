include ../makefile.common
include ../makefile.pin.windows

CXXFLAGS+=$(PIN_CXXFLAGS)

TARGET_OBJS=libcrossdev.o
TARGET_HEADERS=libcrossdev.hpp ../libcrossdev.hpp


all: $(TARGET_OBJS)

.PHONY: clean dist


$(TARGET_OBJS): %.o: %.cpp $(TARGET_HEADERS) 
	$(CXX) $(CXXFLAGS) $(COMP_OBJ)$@ $<

clean:
	rm -f *.o *.pdb
