CC=gcc
CFLAGS=-Wall

all: reactivity sol-reactivity concurrent_threads sol-concurrent_threads
    
reactivity: reactivity.c performance.c performance.h
	$(CC) $(CFLAGS) -o reactivity reactivity.c performance.c -lrt -lm -lpthread
    
sol-reactivity: sol-reactivity.c performance.c performance.h
	$(CC) $(CFLAGS) -o sol-reactivity sol-reactivity.c performance.c -lrt -lm -lpthread

concurrent_threads: concurrent_threads.c performance.c performance.h
	$(CC) $(CFLAGS) -o concurrent_threads concurrent_threads.c performance.c -lm -lpthread
    
sol-concurrent_threads: sol-concurrent_threads.c performance.c performance.h
	$(CC) $(CFLAGS) -o sol-concurrent_threads sol-concurrent_threads.c performance.c -lm -lpthread
    
.PHONY: clean
clean:
	rm -f reactivity sol-reactivity concurrent_threads sol-concurrent_threads
