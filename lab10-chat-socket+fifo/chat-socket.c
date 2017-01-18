#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "common.h"

int shouldStop = 0;

void* receiveMessage(void* arg) {
    int socket_desc = (int)(long)arg;

    char* close_command = CLOSE_COMMAND;
    size_t close_command_len = strlen(close_command);

    /* select() uses sets of descriptors and a timeval interval. The
     * methods returns when either an event occurs on a descriptor in
     * the sets during the given interval, or when that time elapses.
     *
     * The first argument for select is the maximum descriptor among
     * those in the sets plus one. Note also that both the sets and
     * the timeval argument are modified by the call, so you should
     * reinitialize them across multiple invocations.
     *
     * On success, select() returns the number of descriptors in the
     * given sets for which data may be available, or 0 if the timeout
     * expires before any event occurs. */
    struct timeval timeout;
    fd_set read_descriptors;
    int nfds = socket_desc + 1;

    char buf[BUFFER_SIZE];

    while (!shouldStop) {
        int ret;

        // check every 1.5 seconds (why not longer?)
        timeout.tv_sec  = 1;
        timeout.tv_usec = 500000;

        FD_ZERO(&read_descriptors);
        FD_SET(socket_desc, &read_descriptors);

        /** perform select() **/
        ret = select(nfds, &read_descriptors, NULL, NULL, &timeout);

        if (ret == -1 && errno == EINTR) continue;
        ERROR_HELPER(ret, "Unable to select()");

        if (ret == 0) continue; // timeout expired
        
        // at this point (ret==1) our message has been received!
        
        // read available data one byte at a time till '\n' is found
        int bytes_read = 0;
        
        while (1) {
            ret = recv(socket_desc, buf + bytes_read, 1, 0);
            
            if (ret == -1 && errno == EINTR) continue;
            ERROR_HELPER(ret, "Cannot read from socket");
            
            if (ret == 0) {
                fprintf(stderr, "[WARNING] Endpoint closed the connection unexpectedly. Exiting...\n");
                shouldStop = 1;
                pthread_exit(NULL);
            }
            
            // we use post-increment on bytes_read so we first read the
            // byte that has just been written and then we increment
            if (buf[bytes_read++] == '\n') break;
        }
        
        // if we have just received a BYE, we need to update shouldStop!
        // (note that we subtract 1 to skip the message delimiter '\n') 
        if (bytes_read - 1 == close_command_len && !memcmp(buf, close_command, close_command_len)) {
            fprintf(stderr, "Chat session terminated from endpoint. Please press ENTER to exit.\n");
            shouldStop = 1;
        } else {
            // print received message
            buf[bytes_read] = '\0';
            printf("==> %s", buf);
        }
    }

    pthread_exit(NULL);
}

void* sendMessage(void* arg) {
    int socket_desc = (int)(long)arg;

    char* close_command = CLOSE_COMMAND;
    size_t close_command_len = strlen(close_command);

    char buf[BUFFER_SIZE];

    while (!shouldStop) {
        /* Read a line from stdin: fgets() reads up to sizeof(buf)-1
         * bytes and on success returns the first argument passed.
         * Note that '\n' is added at the end of the message when ENTER
         * is pressed: we can thus use it as our message delimiter! */
        if (fgets(buf, sizeof(buf), stdin) != (char*)buf) {
            fprintf(stderr, "Error while reading from stdin, exiting...\n");
            exit(EXIT_FAILURE);
        }

        // check if the endpoint has closed the connection
        if (shouldStop) break;

        // compute number of bytes to send (skip string terminator '\0')
        size_t msg_len = strlen(buf);
        
        int ret, bytes_sent = 0;

        // make sure that all bytes are sent!
        while (bytes_sent < msg_len) {
            ret = send(socket_desc, buf + bytes_sent, msg_len - bytes_sent, 0);
            if (ret == -1 && errno == EINTR) continue;
            ERROR_HELPER(ret, "Cannot write to socket");
            bytes_sent += ret;
        }

        // if we just sent a BYE command, we need to update shouldStop!
        // (note that we subtract 1 to skip the message delimiter '\n')
        if (msg_len - 1 == close_command_len && !memcmp(buf, close_command, close_command_len)) {
            shouldStop = 1;
            fprintf(stderr, "Chat session terminated.\n");
        }
    }

    pthread_exit(NULL);
}

void chatSession(int socket_desc) {
    int ret;

    fprintf(stderr, "Chat session started! Send %s to close it.\n", CLOSE_COMMAND);

    pthread_t chat_threads[2];

    ret = pthread_create(&chat_threads[0], NULL, receiveMessage, (void*)(long)socket_desc);
    PTHREAD_ERROR_HELPER(ret, "Cannot create thread for receiving messages");

    ret = pthread_create(&chat_threads[1], NULL, sendMessage, (void*)(long)socket_desc);
    PTHREAD_ERROR_HELPER(ret, "Cannot create thread for sending messages");

    // wait for termination
    ret = pthread_join(chat_threads[0], NULL);
    PTHREAD_ERROR_HELPER(ret, "Cannot join on thread for receiving messages");

    ret = pthread_join(chat_threads[1], NULL);
    PTHREAD_ERROR_HELPER(ret, "Cannot join on thread for sending messages");

    // close socket
    ret = close(socket_desc);
    ERROR_HELPER(ret, "Cannot close socket");
}

// executed when user specifies a "connect" command
inline void connectTo(in_addr_t ip_addr, uint16_t port_number_no) {
    int ret;
    int socket_desc;
    struct sockaddr_in server_addr = {0}; // some fields are required to be filled with 0

    // create socket
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    ERROR_HELPER(socket_desc, "Could not create socket");

    // set up parameters for the connection
    server_addr.sin_addr.s_addr = ip_addr;
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = port_number_no;

    // initiate a connection on the socket
    ret = connect(socket_desc, (struct sockaddr*) &server_addr, sizeof(struct sockaddr_in));
    ERROR_HELPER(ret, "Could not create connection");

    // start a chat session
    chatSession(socket_desc);
}

// executed when user specifies an "accept" command
inline void listenOnPort(uint16_t port_number_no) {
    int ret;
    int server_desc, client_desc;

    struct sockaddr_in server_addr = {0}, client_addr = {0};
    int sockaddr_len = sizeof(struct sockaddr_in); // see accept()

    // initialize socket for listening
    server_desc = socket(AF_INET , SOCK_STREAM , 0);
    ERROR_HELPER(server_desc, "Could not create socket");

    server_addr.sin_addr.s_addr = INADDR_ANY; // we want to accept connections from any interface
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = port_number_no;

    // We enable SO_REUSEADDR to quickly restart our server after a crash
    int reuseaddr_opt = 1;
    ret = setsockopt(server_desc, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt, sizeof(reuseaddr_opt));
    ERROR_HELPER(ret, "Cannot set SO_REUSEADDR option");

    // bind address to socket
    ret = bind(server_desc, (struct sockaddr*) &server_addr, sockaddr_len);
    ERROR_HELPER(ret, "Cannot bind address to socket");

    // start listening: backlog is zero since the program dies when a chat has terminated!
    ret = listen(server_desc, 0);
    ERROR_HELPER(ret, "Cannot listen on socket");

    // accept incoming connection (cycle required to check for interruption by signals)
    while (1) {
        client_desc = accept(server_desc, (struct sockaddr*)&client_addr, (socklen_t*) &sockaddr_len);
        if (client_desc == -1 && errno == EINTR) continue;

        // start a chat session
        chatSession(client_desc);

        // chat has terminated: program should exit
        ret = close(server_desc);
        ERROR_HELPER(ret, "Cannot close listening socket");
        return;
    }
}

void syntaxError(char* prog_name) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "       %s accept <port_number>\n", prog_name);
    fprintf(stderr, "  OR:\n");
    fprintf(stderr, "       %s connect <IP_address> <port_number>\n", prog_name);
    exit(EXIT_FAILURE);
}

int main(int argc, char* argv[]) {
    if (argc == 3) {
        // accept incoming connection on the given port
        if (strcmp(argv[1], "accept")) syntaxError(argv[0]);

        uint16_t port_number_no; // we use network byte order

        long tmp = strtol(argv[2], NULL, 0); // safer than atoi()
        if (tmp < 1024 || tmp > 49151) {
            fprintf(stderr, "Please use a port number between 1024 and 49151.\n");
            exit(EXIT_FAILURE);
        }
        port_number_no = htons((uint16_t)tmp);

        listenOnPort(port_number_no);
    } else if (argc == 4) {
        // connect to a host
        if (strcmp(argv[1], "connect")) syntaxError(argv[0]);

        // we use network byte order
        in_addr_t ip_addr;
        uint16_t port_number_no;

        // retrieve IP address
        ip_addr = inet_addr(argv[2]); // we omit error checking

        // retrieve port number
        long tmp = strtol(argv[3], NULL, 0); // safer than atoi()
        if (tmp < 1024 || tmp > 49151) {
            fprintf(stderr, "Please use a port number between 1024 and 49151.\n");
            exit(EXIT_FAILURE);
        }
        port_number_no = htons((uint16_t)tmp);

        connectTo(ip_addr, port_number_no);
    } else {
        syntaxError(argv[0]);
    }

    exit(EXIT_SUCCESS);
}
