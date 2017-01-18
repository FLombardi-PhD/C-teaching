CC = gcc -Wall -g
LDFLAGS = -lpthread

all: server client

server: server.c util.h util.c
	$(CC) -o server server.c util.c $(LDFLAGS)

client: client.c
	$(CC) -o client client.c $(LDFLAGS)

.PHONY: clean
clean:
	rm -f client server

