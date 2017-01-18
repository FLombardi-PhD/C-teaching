CC = gcc -Wall -g

all: echo client

echo: echo.c common.h
	$(CC) -o echo echo.c

client: client.c common.h
	$(CC) -o client client.c

.PHONY: clean

clean:
	rm -f echo client
