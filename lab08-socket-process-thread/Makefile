CC = gcc -Wall -g

all: base client multiprocess multithread

base: base.c common.h
	$(CC) -o base base.c

client: client.c common.h
	$(CC) -o client client.c

multiprocess: multiprocess.c common.h
	$(CC) -o multiprocess multiprocess.c

# do not forget to link the binary against libpthread!
multithread: multithread.c common.h
	$(CC) -o multithread multithread.c -lpthread

.PHONY: clean

clean:
	rm -f base client multiprocess multithread
