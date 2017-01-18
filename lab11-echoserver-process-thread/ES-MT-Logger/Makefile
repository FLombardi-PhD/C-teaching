CC = gcc -Wall -g

all: echo_client echo_client_mt echo_server_mt_logger

echo_client: echo_client.c common.h
	$(CC) -o echo_client echo_client.c

echo_client_mt: echo_client_mt.c common.h
	$(CC) -o echo_client_mt echo_client_mt.c -lpthread

echo_server_mt_logger: echo_server_mt_logger.c common.h
	$(CC) -o echo_server_mt_logger echo_server_mt_logger.c -lpthread

.PHONY: clean
clean:
	rm -f echo_client echo_client_mt echo_server_mt_logger log.txt
