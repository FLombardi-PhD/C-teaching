#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <semaphore.h>
#include <unistd.h>
#include <arpa/inet.h>  // htons()
#include <netinet/in.h> // struct sockaddr_in
#include <sys/socket.h>

#include "common.h"

#define LOG_BUFFER_SIZE 128

/**
 * We use a circular buffer to store the items that are produced and
 * eventually consumed. Each item is a char* string dynamically
 * allocated in a producer thread using my_log(), and consumed by the
 * consumer thread whose code is contained inside logger(). Semaphores
 * are sem_t global variables allocated in the program's data segment.
 **/
char* log_buffer[LOG_BUFFER_SIZE];  // circular buffer of char* elements

sem_t fill_count;   // to check if any new data is available for processing
sem_t empty_count;  // to check if there are any available slots to write new data

sem_t write_mutex;  // to avoid race conditions between two producers

int read_index;     // index of the next slot containing information to be read
int write_index;    // index of the next available slot for writing

int logfile_desc;   // file descriptor for logger thread is opened inside main()

typedef struct handler_args_s {
    int socket_desc;
    struct sockaddr_in* client_addr;
} handler_args_t;

void my_log(const char* msg) {
    // duplicate msg string (adding a line terminator for the log file)
    char *tmp = (char*)malloc(strlen(msg) + 2);
    sprintf(tmp, "%s\n", msg);

    /** [SOLUTION]
     * This is the producer side:
     * - wait for the availability of empty slots
     * - manage concurrent accesses from other instances of producers
     * - insert the log message and update write pointer (i.e., index)
     * - signal the consumer that another filled slot is available
     * - take into account the fact the allocated memory has to be
     *   released sooner or later....
     **/
    int ret = sem_wait(&empty_count);
    ERROR_HELPER(ret, "Wait on empty_count failed");

    ret = sem_wait(&write_mutex);
    ERROR_HELPER(ret, "Wait on write_mutex failed");

    // write the item and update write_index accordingly
    log_buffer[write_index] = tmp;
    write_index = (write_index + 1) % LOG_BUFFER_SIZE;

    ret = sem_post(&write_mutex);
    ERROR_HELPER(ret, "Post on write_mutex failed");

    ret = sem_post(&fill_count);
    ERROR_HELPER(ret, "Post on fill_count failed");
}

void* logger(void *args) {

    while (1) {
        /** [SOLUTION]
         * This the consumer side:
         * - wait for the availability of filled slots
         * - get next message to log and update read pointer
         * - the "consume part" is already implemented (write to log file)
         * - there is something to release here...
         * - signal the availability of a new empty slot
         **/
        int ret = sem_wait(&fill_count);
        ERROR_HELPER(ret, "Wait on fill_count failed");
        
        // get the log message and update read_index accordingly
        char *log_msg = log_buffer[read_index];
        read_index = (read_index + 1) % LOG_BUFFER_SIZE;

        // write data on the log file
        int written_bytes = 0;
        int bytes_left = strlen(log_msg);
        while (bytes_left > 0) {
            ret = write(logfile_desc, log_msg + written_bytes, bytes_left);

            // handle errors
            if (ret == -1 && errno == EINTR) continue;
            ERROR_HELPER(ret, "Cannot write to log file");

            bytes_left -= ret;
            written_bytes += ret;
        }
        
        free(log_msg); // we free the memory allocated by my_log()

        ret = sem_post(&empty_count);
        ERROR_HELPER(ret, "Post on empty_count failed");
    }

}

void* connection_handler(void* arg) {
    handler_args_t* args = (handler_args_t*)arg;

    int ret, recv_bytes;

    char buf[DEFAULT_BUFFER_SIZE];
    char log_msg[DEFAULT_BUFFER_SIZE];
    int msg_len;

    char* quit_command = SERVER_COMMAND;
    size_t quit_command_len = strlen(quit_command);

    // parse client IP address and port
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(args->client_addr->sin_addr), client_ip, INET_ADDRSTRLEN);
    uint16_t client_port = ntohs(args->client_addr->sin_port); // port number is an unsigned short

    // send welcome message
    sprintf(buf, "Hi! I'm an echo server. You are %s talking on port %hu.\nI will send you back whatever"
            " you send me. I will stop if you send me %s :-)\n", client_ip, client_port, quit_command);
    msg_len = strlen(buf);
    int bytes_sent = 0;
    while (bytes_sent < msg_len) {
        ret = send(args->socket_desc, buf+bytes_sent, msg_len-bytes_sent, 0);
        if (ret == 1 && errno == EINTR) continue;
        ERROR_HELPER(ret, "Cannot write to socket");
        bytes_sent += ret;
    }

    // echo loop
    while (1) {
        // read message from client
        // (best-effort implementation: we don't have a message delimiter)
        while ( (recv_bytes = recv(args->socket_desc, buf, DEFAULT_BUFFER_SIZE, 0)) <= 0 ) {
            if (recv_bytes == 0) ERROR_HELPER(-1, "Connection closed unexpectedly!");
            
            // if we get here we know that ret == -1
            if (errno == EINTR) continue;
            ERROR_HELPER(-1, "Cannot read from socket");
        }

        // record log message
        buf[recv_bytes] = '\0';
        snprintf(log_msg, sizeof(log_msg), "Message received from client %s:%hu: %s", client_ip, client_port, buf);
        my_log(log_msg);

        // check whether I have just been told to quit...
        if (recv_bytes == quit_command_len && !memcmp(buf, quit_command, quit_command_len)) break;

        // ... or if I have to send the message back
        bytes_sent = 0;
        while (bytes_sent < recv_bytes) {
            ret = send(args->socket_desc, buf+bytes_sent, recv_bytes-bytes_sent, 0);
            if (ret == 1 && errno == EINTR) continue;
            ERROR_HELPER(ret, "Cannot write to socket");
            bytes_sent += ret;
        }
    }

    // close socket
    ret = close(args->socket_desc);
    ERROR_HELPER(ret, "Cannot close socket for incoming connection");

    sprintf(log_msg, "Thread created to handle the client %s:%hu has completed", client_ip, client_port);
    my_log(log_msg);

    free(args->client_addr); // do not forget to free this buffer!
    free(args);
    pthread_exit(NULL);
}

int main(int argc, char* argv[]) {
    int ret;

    int socket_desc, client_desc;

    // some fields are required to be filled with 0
    struct sockaddr_in server_addr = {0};

    int sockaddr_len = sizeof(struct sockaddr_in); // we will reuse it for accept()

    // initialize socket for listening
    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    ERROR_HELPER(socket_desc, "Could not create socket");

    server_addr.sin_addr.s_addr = INADDR_ANY; // we want to accept connections from any interface
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(SERVER_PORT); // don't forget about network byte order!

    /* We enable SO_REUSEADDR to quickly restart our server after a crash:
     * for more details, read about the TIME_WAIT state in the TCP protocol */
    int reuseaddr_opt = 1;
    ret = setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt, sizeof(reuseaddr_opt));
    ERROR_HELPER(ret, "Cannot set SO_REUSEADDR option");

    // bind address to socket
    ret = bind(socket_desc, (struct sockaddr*) &server_addr, sockaddr_len);
    ERROR_HELPER(ret, "Cannot bind address to socket");

    // start listening
    ret = listen(socket_desc, MAX_CONN_QUEUE);
    ERROR_HELPER(ret, "Cannot listen on socket");

    /** [SOLUTION]
     * initialize read and write indexes
     **/
    read_index  = 0;
    write_index = 0;

    /** [SOLUTION]
     * initialize semaphores
     **/
    ret = sem_init(&fill_count, 0, 0);
    ERROR_HELPER(ret, "Could not initialize fill_count");

    ret = sem_init(&empty_count, 0, LOG_BUFFER_SIZE);
    ERROR_HELPER(ret, "Could not initialize empty_count");

    ret = sem_init(&write_mutex, 0, 1);
    ERROR_HELPER(ret, "Could not initialize write_mutex");

    // open log file
    logfile_desc = open(LOGFILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    ERROR_HELPER(logfile_desc, "Could not create logging file");

    // start logger thread
    pthread_t thread;
	
    ret = pthread_create(&thread, NULL, logger, NULL);
    PTHREAD_ERROR_HELPER(ret, "[MAIN THREAD] Cannot create a new thread");
	
    pthread_detach(thread);
	PTHREAD_ERROR_HELPER(ret, "Could not detach the thread");
	
    // we allocate client_addr dynamically and initialize it to zero
    struct sockaddr_in* client_addr = calloc(1, sizeof(struct sockaddr_in));

    // loop to manage incoming connections spawning handler threads
    while (1) {
        // accept incoming connection
        client_desc = accept(socket_desc, (struct sockaddr*) client_addr, (socklen_t*) &sockaddr_len);
        if (client_desc == -1 && errno == EINTR) continue; // check for interruption by signals
        ERROR_HELPER(client_desc, "Cannot open socket for incoming connection");

        my_log("Incoming connection accepted");

        // put arguments for the new thread into a buffer
        handler_args_t* thread_args = malloc(sizeof(handler_args_t));
        thread_args->socket_desc = client_desc;
        thread_args->client_addr = client_addr;

        ret = pthread_create(&thread, NULL, connection_handler, (void*)thread_args);
		PTHREAD_ERROR_HELPER(ret, "[MAIN THREAD] Cannot create a new thread");

        my_log("New thread created to handle the request");

        pthread_detach(thread); // I won't phtread_join() on this thread
		PTHREAD_ERROR_HELPER(ret, "Could not detach the thread"); 

        // we can't just reset fields: we need a new buffer for client_addr!
        client_addr = calloc(1, sizeof(struct sockaddr_in));
    }

    exit(EXIT_SUCCESS); // this will never be executed
}
