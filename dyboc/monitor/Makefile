CFLAGS=-O3 -Wall -Werror -DMONITOR_DEBUG -I../libpmalloc

#DFLAGS=-DMONITOR_DEBUG -DNDEBUG

MONITOR_OBJS=monitor.o child.o


.PHONY: all clean

all: monitor

monitor: $(MONITOR_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(MONITOR_OBJS) monitor
