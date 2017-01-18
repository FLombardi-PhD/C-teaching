#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>  // htons()
#include <netinet/in.h> // struct sockaddr_in
#include <sys/socket.h>
#include <sys/syscall.h> // gettid()

#include "common.h" // MAX_CONCURRENCY, ERROR_HELPER and other macros

/** Global data **/
sem_t connections;

/** Method executed by the server when it receives a INT or TERM signal **/
void signalHandlerCleanup(int sig_no) {
    fprintf(stderr, "Performing cleanup before exiting... ");

    /* ===> SOLUTION <=== */
    /** Destroy the semaphore used to control the degree of concurrency **/
    int ret = sem_destroy(&connections);
    ERROR_HELPER(ret, "Cannot destroy semaphore");

    fprintf(stderr, "Success!\n");
    exit(EXIT_SUCCESS);
}

/* Data structure to encapsulate arguments for handler threads */
typedef struct handler_args_s {
    int socket_desc;
    struct sockaddr_in* client_addr;
} handler_args_t;

/* Method executed by threads created to handle incoming connections */
void* connection_handler(void* arg) {
    handler_args_t* args = (handler_args_t*)arg;

    // retrieve current thread's ID (TID is unique in the system)
    pid_t thread_id = syscall(SYS_gettid);

    /* We make local copies of the fields from the handler's arguments
     * data structure only to share as much code as possible with the
     * other two versions of the server. In general this is not a good
     * coding practice: using simple indirection is better! */
    int socket_desc = args->socket_desc;
    struct sockaddr_in* client_addr = args->client_addr;

    int ret, recv_bytes;

    char buf[1024];
    size_t buf_len = sizeof(buf);
    int msg_len;

    char* quit_command = SERVER_COMMAND;
    size_t quit_command_len = strlen(quit_command);

    // parse client IP address and port
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr->sin_addr), client_ip, INET_ADDRSTRLEN);
    uint16_t client_port = ntohs(client_addr->sin_port); // port number is an unsigned short

    // message for the log file
    fprintf(stderr, "[THREAD %u] Handling connection from %s on port %hu...\n", thread_id, client_ip, client_port);

    // send welcome message
    sprintf(buf, "Hi! I'm an echo server. You are %s talking on port %hu.\nI will send you back whatever"
            " you send me. I will stop if you send me %s :-)\n", client_ip, client_port, quit_command);
    msg_len = strlen(buf);
    int bytes_sent = 0;
    while (bytes_sent < msg_len) {
        ret = send(socket_desc, buf+bytes_sent, msg_len-bytes_sent, 0);
        if (ret == 1 && errno == EINTR) continue;
        ERROR_HELPER(ret, "Cannot write to socket");
        bytes_sent += ret;
    }

    // echo loop
    while (1) {
        // read message from client
        // (best-effort implementation: we don't have a message delimiter)
        while ( (recv_bytes = recv(socket_desc, buf, buf_len, 0)) <= 0 ) {
            if (recv_bytes == 0) ERROR_HELPER(-1, "Connection closed unexpectedly!");
            
            // if we get here we know that ret == -1
            if (errno == EINTR) continue;
            ERROR_HELPER(-1, "Cannot read from socket");
        }

        // check whether I have just been told to quit...
        if (recv_bytes == quit_command_len && !memcmp(buf, quit_command, quit_command_len)) break;

        // ... or if I have to send the message back
        bytes_sent = 0;
        while (bytes_sent < recv_bytes) {
            ret = send(socket_desc, buf+bytes_sent, recv_bytes-bytes_sent, 0);
            if (ret == 1 && errno == EINTR) continue;
            ERROR_HELPER(ret, "Cannot write to socket");
            bytes_sent += ret;
        }
    }

    // close socket
    ret = close(socket_desc);
    ERROR_HELPER(ret, "Cannot close socket for incoming connection");

    fprintf(stderr, "[THREAD %u] Connection with %s on port %hu closed.\n", thread_id, client_ip, client_port);

    /* ===> SOLUTION <=== */
    /** Thread is about to exit, thus we can update the semaphore **/
    ret = sem_post(&connections);
    ERROR_HELPER(ret, "Post on semaphore failed");

    free(args->client_addr);
    free(args);
    pthread_exit(NULL);
}

int main(int argc, char* argv[]) {
    int ret;

    int socket_desc, client_desc;

    /* ===> SOLUTION <=== */
    /** We set up a semaphore to control server's degree of concurrency
     *  (i.e., the maximum number of connections to handle in parallel) **/
    ret = sem_init(&connections, 0, MAX_CONCURRENCY);
    ERROR_HELPER(ret, "Cannot create semaphore");

    /** Here we set up a handler for SIGTERM and SIGINT signals: this
     *  will allow the server to cleanup before exiting. */
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = &signalHandlerCleanup;
    ret = sigaction(SIGTERM, &action, NULL);
    ERROR_HELPER(ret, "Cannot set up handler for SIGTERM");
    ret = sigaction(SIGINT, &action, NULL);
    ERROR_HELPER(ret, "Cannot set up handler for SIGINT");

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

    // print server boot message
    time_t curr_time;
    time(&curr_time);
    fprintf(stderr, "[MAIN THREAD] Starting server at %s", ctime(&curr_time));

    // we allocate client_addr dynamically and initialize it to zero
    struct sockaddr_in* client_addr = calloc(1, sizeof(struct sockaddr_in));

    // loop to manage incoming connections spawning handler threads
    while (1) {
        // accept incoming connection
        client_desc = accept(socket_desc, (struct sockaddr*) client_addr, (socklen_t*) &sockaddr_len);
        if (client_desc == -1 && errno == EINTR) continue; // check for interruption by signals
        ERROR_HELPER(client_desc, "[MAIN THREAD] Cannot open socket for incoming connection");

        if (DEBUG) fprintf(stderr, "[MAIN THREAD] Incoming connection accepted\n");

        pthread_t thread;

        // put arguments for the new thread into a buffer
        handler_args_t* thread_args = malloc(sizeof(handler_args_t));
        thread_args->socket_desc = client_desc;
        thread_args->client_addr = client_addr;

        /* ===> SOLUTION <=== */
        /** We don't want to create more than MAX_CONCURRENCY threads **/
        ret = sem_wait(&connections);
        ERROR_HELPER(ret, "Wait on semaphore failed");

		ret = pthread_create(&thread, NULL, connection_handler, (void*)thread_args);
		PTHREAD_ERROR_HELPER(ret, "[MAIN THREAD] Cannot create a new thread");
		
		ret = pthread_detach(thread);
		PTHREAD_ERROR_HELPER(ret, "Could not detach the thread");
		
        // we can't just reset fields: we need a new buffer for client_addr!
        client_addr = calloc(1, sizeof(struct sockaddr_in));
    }

    exit(EXIT_SUCCESS); // this will never be reached
}
