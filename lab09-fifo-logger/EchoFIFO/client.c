#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> // mkfifo()
#include <sys/stat.h>  // mkfifo()

#include "common.h"

/** Client component **/
int main(int argc, char* argv[]) {
    int ret;
    int echo_fifo, client_fifo;
    int bytes_left, bytes_sent, bytes_read;
    char buf[1024];

    char* quit_command = QUIT_COMMAND;
    size_t quit_command_len = strlen(quit_command);

    /** [SOLUTION] OPEN THE TWO FIFOS
     *
     * Suggestions:
     * - the two FIFOs should be opened in the same order in both the
     *   Echo and the Client programs to avoid deadlocks
     * - open in 'O_WRONLY' or 'O_RDONLY' mode to fullfil the following
     *   requirement: the Echo program sends data through 'echo_fifo'
     *   and the Client program does it through 'client_fifo'
     **/
     
    /* It is important that the two FIFOs are opened in the same order
     * in both the Echo and the Client process. */
    echo_fifo = open(ECHO_FIFO_NAME, O_RDONLY);
    ERROR_HELPER(echo_fifo, "Cannot open Echo FIFO for reading");
    client_fifo = open(CLNT_FIFO_NAME, O_WRONLY);
    ERROR_HELPER(client_fifo, "Cannot open Client FIFO for writing");

    // display welcome message received from the Echo process
    /** [SOLUTION] READ THE MESSAGE THROUGH THE ECHO FIFO
     *
     * Suggestions:
     * - you can read from a FIFO as from a regular file descriptor
     * - since you don't know the length of the message, just try to
     *   read enough data to fill the buffer (up to, but not including
     *   its last byte, as we later add a string terminator to data)
     * - repeat the read() in case we get interrupted before read()
     *   could write any byte to 'buf'
     * - store the number of bytes read in 'bytes_read'
     * - reading 0 bytes means that the other process has closed
     *   the FIFO unexpectedly: this is an error to deal with!
     **/
    while ( (bytes_read = read(echo_fifo, buf, sizeof(buf) - 1)) <= 0 ) {
        if (bytes_read == 0) {
            fprintf(stderr, "Echo process has closed the Echo FIFO unexpectedly! Exiting...\n");
            exit(EXIT_FAILURE);
        }
        if (errno == EINTR) continue;
        ERROR_HELPER(-1, "Cannot read from Echo FIFO");
    }

    buf[bytes_read] = '\0';
    printf("%s", buf);

    // main loop
    while (1) {
        printf("Insert your message: ");

        // read a line from stdin (including newline symbol '\n')
        if (fgets(buf, sizeof(buf), stdin) != (char*)buf) {
            fprintf(stderr, "Error while reading from stdin, exiting...\n");
            exit(EXIT_FAILURE);
        }

        // send message to Echo process
        bytes_left = strlen(buf) - 1; // discard '\n' from the end of the message
        bytes_sent = 0;
        /** [SOLUTION] SEND THE MESSAGE THROUGH THE CLIENT FIFO
         *
         * Suggestions:
         * - you can write on the FIFO as on a regular file descriptor
         * - make sure that all the bytes have been written: use a while
         *   cycle in the implementation as we did for file descriptors!
         * - store the total number of bytes sent in 'bytes_sent'
         **/
        while (bytes_left > 0) {
            ret = write(client_fifo, buf + bytes_sent, bytes_left);
            if (ret == -1 && errno == EINTR) continue;
            ERROR_HELPER(ret, "Cannot write to Client FIFO");

            bytes_left -= ret;
            bytes_sent += ret;
        }

        /* After a quit command we won't receive data from the server
         * anymore, thus we must exit the main loop. */
        if (bytes_sent == quit_command_len && !memcmp(buf, quit_command, quit_command_len)) break;

        // read message from Echo process
        /** [SOLUTION] READ THE MESSAGE THROUGH THE ECHO FIFO
         *
         * Suggestions:
         * - you can read from a FIFO as from a regular file descriptor
         * - in general you might not know the length of the message,
         *   so just read the data available in the FIFO (and make sure
         *   that we have enough room to add a string terminator later)
         * - repeat the read() when interrupted before reading any data
         * - store the number of bytes read in 'bytes_read'
         * - reading 0 bytes means that the other process has closed
         *   the FIFO unexpectedly: this is an error to deal with!
         **/
        while ( (bytes_read = read(echo_fifo, buf, sizeof(buf) - 1)) <= 0 ) {
            if (bytes_read == 0) {
                fprintf(stderr, "Echo process has closed the Echo FIFO unexpectedly! Exiting...\n");
                exit(EXIT_FAILURE);
            }
            if (errno == EINTR) continue;
            ERROR_HELPER(-1, "Cannot read from Echo FIFO");
        } 
        
        buf[bytes_read] = '\0';
        printf("Server response: %s\n", buf);
    }

    // close the descriptors
    ret = close(echo_fifo);
    ERROR_HELPER(ret, "Cannot close Echo FIFO");
    ret = close(client_fifo);
    ERROR_HELPER(ret, "Cannot close Client FIFO");

    exit(EXIT_SUCCESS);
}
