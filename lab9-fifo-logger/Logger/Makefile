CC = gcc -Wall -g

all: client server

client: client.c common.h
	$(CC) -o client client.c

# do not forget to link the binary against libpthread!
server: server.c common.h
	$(CC) -o server server.c -lpthread

.PHONY: clean

clean:
	rm -f client server log.txt
