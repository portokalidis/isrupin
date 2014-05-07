TESTS=test1 test2

CC=clang
CFLAGS=-emit-llvm
OPT=opt 

all: $(TESTS:=.bc)

%.bc: %.c
	$(CC) $(CFLAGS) -o $@ -c $^
	#$(OPT) -load $(TOOL).so < $@

clean:
	rm -f *.bc
