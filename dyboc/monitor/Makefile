CFLAGS:=-O2 -Wall -Werror -I../libpmalloc 
#CFLAGS+=-DMONITOR_DEBUG 

MONITOR_OBJS:=monitor.o child.o


.PHONY: all clean

all: monitor

monitor: $(MONITOR_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	cd tests && make clean
	rm -f $(MONITOR_OBJS) monitor
