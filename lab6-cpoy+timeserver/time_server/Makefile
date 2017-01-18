CC = gcc -Wall -g
LDFLAGS =

all: server client

server: server.c common.h
	$(CC) -o server server.c $(LDFLAGS)

client: client.c common.h
	$(CC) -o client client.c $(LDFLAGS)

.PHONY: clean
clean:
	rm -f client server
