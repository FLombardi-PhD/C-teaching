#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>  // htons() and inet_addr()
#include <netinet/in.h> // struct sockaddr_in
#include <sys/socket.h>
#include <time.h>       // nanosleep()
#include <pthread.h>

#include "common.h"

#define THREAD_COUNT    10
#define SLEEP_TIME      100 // in milliseconds

struct timespec pause_interval; // used by nanosleep()
int should_stop;

void* connection_handler(void* arg) {

    int thread_idx = (int)(long)arg;
    int ret;

    // variables for handling a socket
    int socket_desc;
    struct sockaddr_in server_addr = {0}; // some fields are required to be filled with 0

    // create a socket
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    ERROR_HELPER(socket_desc, "Could not create socket");

    // set up parameters for the connection
    server_addr.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(SERVER_PORT); // don't forget about network byte order!

    // initiate a connection on the socket
    ret = connect(socket_desc, (struct sockaddr*) &server_addr, sizeof(struct sockaddr_in));
    ERROR_HELPER(ret, "Could not create connection");

    char buf[DEFAULT_BUFFER_SIZE];
    int msg_len;

    // get welcome message from server
    while ( (msg_len = recv(socket_desc, buf, DEFAULT_BUFFER_SIZE - 1, 0)) <= 0 ) {
        if (msg_len == 0) ERROR_HELPER(-1, "Connection closed unexpectedly!");
        
        // if we get here we know that ret == -1
        if (errno == EINTR) continue;
        ERROR_HELPER(-1, "Cannot read from socket");
    }

    // main loop
    int counter = 0;
    while (1) {

        nanosleep(&pause_interval, NULL);

        char* quit_command = SERVER_COMMAND;
        size_t quit_command_len = strlen(quit_command);

        if (should_stop)
            sprintf(buf, "%s", quit_command);
        else
            sprintf(buf, "[Thread %d] message #%d", thread_idx, ++counter);
        msg_len = strlen(buf);
        buf[msg_len] = '\0'; // remove '\n' from the end of the message

        // send message to server
        int bytes_sent = 0;
        while (bytes_sent < msg_len) {
            ret = send(socket_desc, buf+bytes_sent, msg_len-bytes_sent, 0);
            if (ret == 1 && errno == EINTR) continue;
            ERROR_HELPER(ret, "Cannot write to socket");
            bytes_sent += ret;
        }

        /* After a quit command we won't receive any more data from
         * the server, thus we must exit the main loop. */
        if (msg_len == quit_command_len && !memcmp(buf, quit_command, quit_command_len)) break;
        
        // read message from server
        // (best-effort implementation: we don't have a message delimiter)
        while ( (msg_len = recv(socket_desc, buf, DEFAULT_BUFFER_SIZE, 0)) <= 0 ) {
            if (msg_len == 0) ERROR_HELPER(-1, "Connection closed unexpectedly!");
            
            // if we get here we know that ret == -1
            if (errno == EINTR) continue;
            ERROR_HELPER(-1, "Cannot read from socket");
        }
    }


    // close the socket
    ret = close(socket_desc);
    ERROR_HELPER(ret, "Cannot close socket");

    pthread_exit(EXIT_SUCCESS);
}

int main(int argc, char* argv[]) {

    /* nanosleep() takes as first argument a pointer to a timespec
     * object containing the desired interval. It also takes a pointer
     * (which can be NULL) to another timespec object where it stores
     * the remaining time if it gets interrupted by a signal. */
    pause_interval.tv_sec = 0;
    pause_interval.tv_nsec = SLEEP_TIME * 1000000; // conversion from milliseconds (10^-3) to nanoseconds (10^-9)
    should_stop = 0;

    int ret, i;
    char buf[DEFAULT_BUFFER_SIZE];

    pthread_t *threads = (pthread_t*)malloc(THREAD_COUNT * sizeof(pthread_t));
    for (i = 0; i < THREAD_COUNT; i++) {
        /* We pass the value (not the address, otherwise we would have a
         * race condition!!!) of i by casting it into a void* type */
        ret = pthread_create(&threads[i], NULL, connection_handler, (void*)(long)i);
        PTHREAD_ERROR_HELPER(ret, "Error creating a new thread");
    }

    printf("There are %d threads running and interacting with the EchoServer every %d milliseconds.\nPress ENTER to stop the threads and exit...", THREAD_COUNT, SLEEP_TIME);

    /* Read a line from stdin
     *
     * fgets() reads up to sizeof(buf)-1 bytes and on success
     * returns the first argument passed to it. */
    if (fgets(buf, sizeof(buf), stdin) != (char*)buf) {
        fprintf(stderr, "Error while reading from stdin, exiting...\n");
        exit(EXIT_FAILURE);
    }

    /* We set the should_stop flag to notify the threads that they
     * have to complete and then we wait for their termination. */
    should_stop = 1;

    for (i = 0; i < THREAD_COUNT; i++)
        pthread_join(threads[i], NULL);
    free(threads);

    exit(EXIT_SUCCESS);
}
