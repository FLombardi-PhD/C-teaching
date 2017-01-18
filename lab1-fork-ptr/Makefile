CC = gcc
CFLAGS = -Wall -g

all: arrayBounds distance nullPtr processes

arrayBounds: sol-arrayBounds.c
	$(CC) $(CFLAGS) -o arrayBounds sol-arrayBounds.c

distance: sol-distance.c
	$(CC) $(CFLAGS) -o distance sol-distance.c -lm

nullPtr: sol-nullPtr.c
	$(CC) $(CFLAGS) -o nullPtr sol-nullPtr.c

processes: sol-processes.c
	$(CC) $(CFLAGS) -o processes sol-processes.c

.PHONY: clean
clean:
	rm -f arrayBounds distance nullPtr processes
