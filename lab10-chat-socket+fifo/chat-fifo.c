#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "common.h"

#define FIFO_ACCEPT_SUFFIX  "_accept"
#define FIFO_CONNECT_SUFFIX "_connect"

char listen_fifo_name[128];
char accept_fifo_name[128];

int shouldStop = 0;

void* receiveMessage(void* arg) {
    int recv_fifo = (int)(long)arg;

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
    int nfds = recv_fifo + 1;

    char buf[BUFFER_SIZE];

    while (!shouldStop) {
        int ret;

        // check every 1.5 seconds (why not longer?)
        timeout.tv_sec  = 1;
        timeout.tv_usec = 500000;

        FD_ZERO(&read_descriptors);
        FD_SET(recv_fifo, &read_descriptors);

        /** perform select() **/
        ret = select(nfds, &read_descriptors, NULL, NULL, &timeout);

        if (ret == -1 && errno == EINTR) continue;
        ERROR_HELPER(ret, "Unable to select()");

        if (ret == 0) continue; // timeout expired

        // at this point (ret==1) our message has been received!
        
        // read available data one byte at a time till '\n' is found
        int bytes_read = 0;
        
        while (1) {
            ret = read(recv_fifo, buf + bytes_read, 1);
            
            if (ret == -1 && errno == EINTR) continue;
            ERROR_HELPER(ret, "Cannot read from FIFO");
            
            if (ret == 0) {
                fprintf(stderr, "[WARNING] Endpoint closed the FIFO unexpectedly. Exiting...\n");
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
    int send_fifo = (int)(long)arg;

    char* close_command = CLOSE_COMMAND;
    size_t close_command_len = strlen(close_command);

    char buf[BUFFER_SIZE];

    while (!shouldStop) {
        /* Read a line from stdin: fgets() reads up to sizeof(buf)-1
         * bytes and on success returns the first argument passed. */
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
            ret = write(send_fifo, buf + bytes_sent, msg_len - bytes_sent);
            if (ret == -1 && errno == EINTR) continue;
            ERROR_HELPER(ret, "Cannot write to FIFO");
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

void chatSession(int send_fifo, int recv_fifo) {
    int ret;

    fprintf(stderr, "Chat session started! Send %s to close it.\n", CLOSE_COMMAND);

    pthread_t chat_threads[2];

    ret = pthread_create(&chat_threads[0], NULL, receiveMessage, (void*)(long)recv_fifo);
    PTHREAD_ERROR_HELPER(ret, "Cannot create thread for receiving messages");

    ret = pthread_create(&chat_threads[1], NULL, sendMessage, (void*)(long)send_fifo);
    PTHREAD_ERROR_HELPER(ret, "Cannot create thread for sending messages");

    // wait for termination
    ret = pthread_join(chat_threads[0], NULL);
    PTHREAD_ERROR_HELPER(ret, "Cannot join on thread for receiving messages");

    ret = pthread_join(chat_threads[1], NULL);
    PTHREAD_ERROR_HELPER(ret, "Cannot join on thread for sending messages");

    // close FIFOs
    ret = close(send_fifo);
    ERROR_HELPER(ret, "Cannot close FIFO used for sending messages");
    ret = close(recv_fifo);
    ERROR_HELPER(ret, "Cannot close FIFO used for receiving messages");
}

// this method reads from Accept FIFO and writes to Listen FIFO
inline void connectOnFIFO() {
    // open FIFOs (Listen FIFO first)
    int recv_fifo, send_fifo;

    send_fifo = open(listen_fifo_name, O_WRONLY);
    ERROR_HELPER(send_fifo, "Cannot open Listen FIFO for writing");
    recv_fifo = open(accept_fifo_name, O_RDONLY);
    ERROR_HELPER(recv_fifo, "Cannot open Accept FIFO for reading");

    // start a chat session
    chatSession(send_fifo, recv_fifo);
}

// this method reads from Listen FIFO and writes to Accept FIFO
inline void listenOnFIFO() {
    int ret;

    // create both FIFOs
    ret = mkfifo(listen_fifo_name, 0666);
    ERROR_HELPER(ret, "Cannot create Listen FIFO");
    ret = mkfifo(accept_fifo_name, 0666);
    ERROR_HELPER(ret, "Cannot create Accept FIFO");

    // open FIFOs (Listen FIFO first)
    int recv_fifo, send_fifo;

    recv_fifo = open(listen_fifo_name, O_RDONLY);
    ERROR_HELPER(recv_fifo, "Cannot open Listen FIFO for reading");
    send_fifo = open(accept_fifo_name, O_WRONLY);
    ERROR_HELPER(send_fifo, "Cannot open Accept FIFO for writing");

    // start a chat session
    chatSession(send_fifo, recv_fifo);

    // remove both FIFOs
    ret = unlink(listen_fifo_name);
    ERROR_HELPER(ret, "Cannot unlink Listen FIFO");
    ret = unlink(accept_fifo_name);
    ERROR_HELPER(ret, "Cannot unlink Accept FIFO");
}

void syntaxError(char* prog_name) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "       %s accept <FIFO_prefix>\n", prog_name);
    fprintf(stderr, "  OR:\n");
    fprintf(stderr, "       %s connect <FIFO_prefix>\n", prog_name);
    exit(EXIT_FAILURE);
}

int main(int argc, char* argv[]) {
    if (argc == 3) {
        // the user specifies a prefix for the names of the two FIFOs we will use
        char* fifo_prefix = argv[2];
        if (!strcmp(argv[1], "accept")) {
            /* accept module will read from Listen FIFO and write to Accept FIFO */
            sprintf(listen_fifo_name, "%s%s", fifo_prefix, FIFO_ACCEPT_SUFFIX);
            sprintf(accept_fifo_name, "%s%s", fifo_prefix, FIFO_CONNECT_SUFFIX);

            listenOnFIFO();
        } else if (!strcmp(argv[1], "connect")) {
            /* connect module will read from Accept FIFO and write to Listen FIFO */
            sprintf(listen_fifo_name, "%s%s", fifo_prefix, FIFO_ACCEPT_SUFFIX);
            sprintf(accept_fifo_name, "%s%s", fifo_prefix, FIFO_CONNECT_SUFFIX);

            connectOnFIFO();
        } else {
            syntaxError(argv[0]);
        }
    } else {
        syntaxError(argv[0]);
    }
    exit(EXIT_SUCCESS);
}
