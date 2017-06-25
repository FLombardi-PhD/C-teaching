#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
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

#include "common.h"

/** Global data **/
pid_t logger_pid;       // Server uses Logger's PID to send it a TERM signal
int logger_shouldStop;  // Logger's signal handler accesses this flag

/** An additional macro to simplify error handling in the Server when
 ** the Logger is active. It will send a SIGTERM signal to the Logger,
 ** which in turn captures it and exits gracefully. **/
#define SRV_ERROR_HELPER_WITH_ACTIVE_LOGGER(ret, message)  do {         \
            if (ret < 0) {                                              \
                fprintf(stderr, "%s: %s\n", message, strerror(errno));  \
                kill(logger_pid, SIGTERM);                              \
                exit(EXIT_FAILURE);                                     \
            }                                                           \
        } while (0)

/** Method executed by the Logger when it catches a INT or TERM signal **/
void signalHandlerForLoggerProcess(int sig_no) {
    /* When we receive a TERM or an INT signal, the while cycle in
     * startLogger() will eventually check the flag logger_shouldStop
     * and close the log file after pending data have been written. */
    logger_shouldStop = 1;
}

/** Core of the Logger process **/
void startLogger(int logfile_desc, int logging_pipe[2]) {
    int ret;

    /** [SOLUTION] SET UP THE PIPE
     *
     * Suggestion: which channel endpoint/direction the Logger needs?
     **/
    /* Logger reads from the pipe while Server writes to it: we close
     * the writing channel endpoint of the pipe on this end. */
    ret = close(logging_pipe[1]);
    ERROR_HELPER(ret, "Cannot close pipe's write descriptor in Logger");

    /* We set up a handler for SIGTERM and SIGINT signals:
     * - our Server sends SIGTERM to the Logger when an error occurs
     * - when we press CTRL-C on the terminal, this will generate a
     *   SIGINT signal: the Server will die, while the Logger will
     *   intercept the signal and exit gracefully */
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = &signalHandlerForLoggerProcess;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);

    /* Main loop */
    char buf[512];
    logger_shouldStop = 0;
    while(1) {
        /** [SOLUTION] PROCESS DATA FROM THE PIPE
         *
         * Suggestions:
         * - we don't know how many bytes the Server will send, but we
         *   can write bytes to the log file as soon as we get them
         * - repeat the read() when interrupted before reading any data
         * - read() will return 0 only when the endpoint is closed
         * - use the variable 'ret' to store the number of bytes read
         **/
        // read available data from the pipe
        ret = read(logging_pipe[0], buf, sizeof(buf));

        if (ret == 0) break; // server closed the pipe: it has died unexpectedly!

        // handle errors
        if (ret == -1 && errno == EINTR) continue;
        ERROR_HELPER(ret, "Cannot read from pipe");

        // write data to the log file
        int written_bytes = 0;
        int bytes_left = ret;
        while (bytes_left > 0) {
            ret = write(logfile_desc, buf + written_bytes, bytes_left);

            // handle errors
            if (ret == -1 && errno == EINTR) continue;
            ERROR_HELPER(ret, "Cannot write to log file");

            bytes_left -= ret;
            written_bytes += ret;
        }

        /* When we reach this point and the flag is true there are no more
         * data to read. Why we do not set it as while's condition? :-) */
        if (logger_shouldStop) break;
    }

    ret = close(logfile_desc);
    ERROR_HELPER(ret, "Cannot close log file from Logger");

    /** [SOLUTION] CLOSE DESCRIPTORS
     *
     * Suggestion: check the return code of close() operation(s)
     */
    close(logging_pipe[0]); // what happens when you write on a closed pipe? 
    ERROR_HELPER(ret, "Cannot close pipe's read descriptor in Logger");
    
    exit(EXIT_SUCCESS);
}

/** Data structure to encapsulate arguments for handler threads **/
typedef struct handler_args_s {
    int socket_desc;
    struct sockaddr_in* client_addr;
} handler_args_t;

/** Method executed by threads created to handle incoming connections **/
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
    size_t msg_len;

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
    while ( (ret = send(socket_desc, buf, msg_len, 0)) < 0 ) {
        if (errno == EINTR) continue;
        SRV_ERROR_HELPER_WITH_ACTIVE_LOGGER(-1, "Cannot write to the socket");
    }

    // echo loop
    while (1) {
        // read message from client
        while ( (recv_bytes = recv(socket_desc, buf, sizeof(buf), 0)) < 0 ) {
            if (errno == EINTR) continue;
            SRV_ERROR_HELPER_WITH_ACTIVE_LOGGER(-1, "Cannot read from socket");
        }

        // check whether I have just been told to quit...
        if (recv_bytes == quit_command_len && !memcmp(buf, quit_command, quit_command_len)) break;

        // ... or if I have to send the message back
        while ( (ret = send(socket_desc, buf, recv_bytes, 0)) < 0 ) {
            if (errno == EINTR) continue;
            SRV_ERROR_HELPER_WITH_ACTIVE_LOGGER(-1, "Cannot write to the socket");
        }
    }

    // close socket
    ret = close(socket_desc);
    SRV_ERROR_HELPER_WITH_ACTIVE_LOGGER(ret, "Cannot close socket for incoming connection");

    fprintf(stderr, "[THREAD %u] Connection with %s on port %hu closed.\n", thread_id, client_ip, client_port);

    free(args->client_addr); // do not forget to free this buffer!
    free(args);
    pthread_exit(NULL);
}

/** Core of the Server process **/
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

    /** Setup child process for logging stderr **/
    char* logfile_name = "log.txt";
    int logfile_desc = open(logfile_name, O_WRONLY | O_CREAT | O_APPEND, 0644);
    ERROR_HELPER(logfile_name, "Could not create logging file");

    /** [SOLUTION] COMPLETE THE FOLLOWING CODE BLOCK
     *
     * Suggestions:
     * - create a pipe storing the descriptors in logging_pipe[2]
     * - close the channel endpoint not in use in the Server, while the
     *   Logger will do it at the beginning of the startLogger() method
     * - you can redirect stderr using dup2(desc, STDERR_FILENO)
     */
    int logging_pipe[2];
    ret = pipe(logging_pipe);
    ERROR_HELPER(ret, "Cannot create pipe");

    logger_pid = fork();
    if (logger_pid == -1) {
        ERROR_HELPER(-1, "Cannot create Logger process");
    } else if (logger_pid == 0) {
        /* The Logger process will run the startLogger() method */
        ret = close(socket_desc);
        ERROR_HELPER(ret, "Cannot close listening socket from Logger");

        startLogger(logfile_desc, logging_pipe);
    } else {
        /* Server process */

        /** The Logger is active! From now on you should use the macro
         ** SRV_ERROR_HELPER_WITH_ACTIVE_LOGGER to handle errors. **/
         
        // close logfile
        ret = close(logfile_desc);
        SRV_ERROR_HELPER_WITH_ACTIVE_LOGGER(ret, "Cannot close log file in Server");

        ret = close(logging_pipe[0]); /** [SOLUTION] **/
        SRV_ERROR_HELPER_WITH_ACTIVE_LOGGER(ret, "Cannot close pipe's read descriptor in Server");
        
        // redirect stderr on the pipe
        ret = dup2(logging_pipe[1], STDERR_FILENO); /** [SOLUTION] **/
        SRV_ERROR_HELPER_WITH_ACTIVE_LOGGER(ret, "Cannot redirect stderr to the pipe's write descriptor in Server");

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
            SRV_ERROR_HELPER_WITH_ACTIVE_LOGGER(client_desc, "[MAIN THREAD] Cannot open socket for incoming connection");

            if (DEBUG) fprintf(stderr, "[MAIN THREAD] Incoming connection accepted...\n");

            pthread_t thread;

            // put arguments for the new thread into a buffer
            handler_args_t* thread_args = malloc(sizeof(handler_args_t));
            thread_args->socket_desc = client_desc;
            thread_args->client_addr = client_addr;

            if (pthread_create(&thread, NULL, connection_handler, (void*)thread_args) != 0) {
                SRV_ERROR_HELPER_WITH_ACTIVE_LOGGER(-1, "[MAIN THREAD] Cannot create a new thread");
                exit(EXIT_FAILURE);
            }

            pthread_detach(thread); // I won't phtread_join() on this thread

            // we can't just reset fields: we need a new buffer for client_addr!
            client_addr = calloc(1, sizeof(struct sockaddr_in));
        }

        /**
         * [SOLUTION] SHOULD WE ADD SOME CODE HERE TOO? :-)
         **/
        ret = close(logging_pipe[1]);
        SRV_ERROR_HELPER_WITH_ACTIVE_LOGGER(ret, "Cannot close pipe's write descriptor in Logger");
    }

    exit(EXIT_SUCCESS); // this will never be reached
}
