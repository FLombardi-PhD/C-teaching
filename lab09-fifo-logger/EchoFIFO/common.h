#ifndef COMMON_H
#define COMMON_H

// macro to simplify error handling
#define GENERIC_ERROR_HELPER(cond, errCode, msg) do {               \
        if (cond) {                                                 \
            fprintf(stderr, "%s: %s\n", msg, strerror(errCode));    \
            exit(EXIT_FAILURE);                                     \
        }                                                           \
    } while(0)

#define ERROR_HELPER(ret, msg)          GENERIC_ERROR_HELPER((ret < 0), errno, msg)
#define PTHREAD_ERROR_HELPER(ret, msg)  GENERIC_ERROR_HELPER((ret != 0), ret, msg)

/* Configuration parameters */
#define DEBUG           1   // display debug messages
#define QUIT_COMMAND    "QUIT"
#define CLNT_FIFO_NAME  "fifo_client"
#define ECHO_FIFO_NAME  "fifo_echo"


#endif
