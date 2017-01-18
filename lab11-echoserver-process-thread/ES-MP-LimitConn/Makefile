CC = gcc -Wall -g

all: client multiprocess multithread

client: client.c common.h
	$(CC) -o client client.c

multiprocess: multiprocess.c common.h
	$(CC) -o multiprocess multiprocess.c -lpthread

multithread: multithread.c common.h
	$(CC) -o multithread multithread.c -lpthread

.PHONY: clean
clean:
	rm -f client multiprocess multithread
