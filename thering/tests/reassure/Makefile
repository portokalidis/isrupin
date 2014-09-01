
LIBS=-lpthread
CFLAGS=-O0 -Wall -g -fexceptions

TESTS=steps_crashes steps_returns ret_overwrite step_repeated \
      fib_threaded_crashes fib_threaded_returns fib_thread_stretch \
      fib_thread_test fib_block_test steps_threaded_crashes versionseq_app \
      fork_app very_long_rp very_long_rp_crash steps_threaded unaligned_crash

TOOLS=ret_overwrite_tool.so version.so invalidate.so versionseq.so fork.so
TOOLS+=find_routines.so




.PHONY: clean

tests: $(TESTS)

#tools: $(TOOLS)


#$(TOOLS): %.so: %.o
	#$(CXX) $(PIN_LDFLAGS) -o $@ $< $(PIN_LIBS)

%.o: %.cpp
	$(CXX) $(PIN_CXXFLAGS) $(CXXFLAGS) -c $^

$(TESTS): %: %.o
	$(CC) $(LDFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f *.o *.obj $(TESTS) $(TOOLS)
