CC = gcc
CFLAGS = -Wall -g
LDFLAGS = -lpthread
SRC = $(wildcard *.c)
BIN = $(patsubst %.c, %, $(SRC))

all : $(BIN)

$(BIN) : % : %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

.PHONY : clean

clean:
	rm -f $(BIN)
