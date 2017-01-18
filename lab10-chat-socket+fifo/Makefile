CC = gcc -Wall -g
LDFLAGS = -lpthread

all: chat-socket chat-fifo

chat-socket: chat-socket.c
	$(CC) -o chat-socket chat-socket.c $(LDFLAGS)

chat-fifo: chat-fifo.c
	$(CC) -o chat-fifo chat-fifo.c $(LDFLAGS)

.PHONY: clean

clean:
	rm -f chat-socket chat-fifo
