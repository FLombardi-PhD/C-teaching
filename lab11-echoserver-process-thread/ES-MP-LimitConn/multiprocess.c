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
pid_t   main_process;
/* ===> SOLUTION <=== */
sem_t*  connections;

/** Method is executed by the main process and all of its children
 *  when a SIGINT or SIGTERM signal is received. **/
void signalHandlerCleanup(int sig_no) {
    // determine current process ID
    pid_t process_id = syscall(SYS_getpid);

    if (process_id == main_process) {
        /* ===> SOLUTION <=== */
        /** The main process is the one that has to close and unlink the
         *  named semaphore. In fact, a pointer returned by sem_open()
         *  is a pointer to memory mapped with mmap() with the
         *  MAP_SHARED flag set (i.e., points to a shared memory). **/
        int ret = sem_close(connections);
        ERROR_HELPER(ret, "[MAIN PROCESS] Cannot close named semaphore");

        ret = sem_unlink(SEMAPHORE_NAME);
        ERROR_HELPER(ret, "[MAIN PROCESS] Cannot unlink named semaphore");

        fprintf(stderr, "[MAIN PROCESS] Main process terminated gracefully\n");
    } else {
        // nothing to do for child processes
        fprintf(stderr, "[PROCESS %u] Child process terminated gracefully\n", process_id);
    }

    exit(EXIT_SUCCESS);
}

/* Method executed by threads created to handle incoming connections */
void connection_handler(int socket_desc, struct sockaddr_in* client_addr) {
    // retrieve PID for child process
    pid_t process_id = syscall(SYS_getpid);

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
    fprintf(stderr, "[PROCESS %u] Handling connection from %s on port %hu...\n", process_id, client_ip, client_port);

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

    fprintf(stderr, "[PROCESS %u] Connection with %s on port %hu closed.\n", process_id, client_ip, client_port);

    /* ===> SOLUTION <=== */
    /** Process is about to exit, thus we can update the semaphore **/
    ret = sem_post(connections);
    ERROR_HELPER(ret, "Post on named semaphore failed");
}

int main(int argc, char* argv[]) {
    int ret;

    int socket_desc, client_desc;

    /* ===> SOLUTION <=== */
    /** We set up a named semaphore to control server's degree of concurrency
     *  (i.e., the maximum number of connections to handle in parallel) **/
    connections = sem_open(SEMAPHORE_NAME, O_CREAT | O_EXCL, 0600, MAX_CONCURRENCY);

    if (connections == SEM_FAILED && errno == EEXIST) {
        fprintf(stderr, "[WARNING] Named semaphore %s already exists\n", SEMAPHORE_NAME);

        ret = sem_unlink(SEMAPHORE_NAME);
        ERROR_HELPER(ret, "Cannot unlink already existing named semaphore");

        // now we can try to create the semaphore again
        connections = sem_open(SEMAPHORE_NAME, O_CREAT | O_EXCL, 0600, MAX_CONCURRENCY);
    }

    if (connections == SEM_FAILED) {
        ERROR_HELPER(-1, "Cannot open named semaphore");
    }

    /** Here we set up a handler for SIGTERM and SIGINT signals: this
     *  will allow the server to cleanup before exiting. Note that we
     *  store the process ID of the main process in a global variable,
     *  so that the signal handler can access it. **/
    main_process = syscall(SYS_getpid);

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
    fprintf(stderr, "[MAIN PROCESS] Starting server with PID %u at %s", main_process, ctime(&curr_time));

    // we allocate client_addr dynamically and initialize it to zero
    struct sockaddr_in* client_addr = calloc(1, sizeof(struct sockaddr_in));

    // loop to manage incoming connections spawning handler threads
    while (1) {
        // accept incoming connection
        client_desc = accept(socket_desc, (struct sockaddr*) client_addr, (socklen_t*) &sockaddr_len);
        if (client_desc == -1 && errno == EINTR) continue; // check for interruption by signals
        ERROR_HELPER(client_desc, "[MAIN PROCESS] Cannot open socket for incoming connection");

        if (DEBUG) fprintf(stderr, "[MAIN PROCESS] Incoming connection accepted\n");

        /* ===> SOLUTION <=== */
        /** We don't want to create more than MAX_CONCURRENCY child processes **/
        ret = sem_wait(connections);
        ERROR_HELPER(ret, "Wait on named semaphore failed");
        
        pid_t pid = fork();
        if (pid == -1) {
            ERROR_HELPER(-1, "[MAIN PROCESS] Cannot fork to handle the request");
        } else if (pid == 0) {
            // child: close the listening socket and process the request
            ret = close(socket_desc);
            if (ret == -1) {
                pid_t child_pid = syscall(SYS_getpid);
                fprintf(stderr, "[PROCESS %u] Cannot close listening socket", child_pid);
                exit(EXIT_FAILURE);
            }

            // start helper method to handle the request
            connection_handler(client_desc, client_addr);

            free(client_addr);
            exit(EXIT_SUCCESS);
        } else {
            // server: close the incoming socket and continue
            ret = close(client_desc);
            ERROR_HELPER(ret, "[MAIN PROCESS] Cannot close incoming socket");

            // reset fields in client_addr
            memset(client_addr, 0, sizeof(struct sockaddr_in));
        }
    }

    exit(EXIT_SUCCESS); // this will never be reached
}
