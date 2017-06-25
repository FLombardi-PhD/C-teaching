#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> // mkfifo()
#include <sys/stat.h>  // mkfifo()

#include "common.h"

/** [SOLUTION] Method to close and remove the two FIFOs
 * 
 * In the original program, instructions in this method were located
 * right after the while(1) loop in the main() function. However, we can
 * run cleanup code also when the Echo module detects that the Client
 * has closed 'client_fifo' unexpectedly.
 * 
 * Note that you were not required to do this. We show this improvement
 * to point out that FIFOs remain in the filesystem if we do not perform
 * unlink() on them once all the descriptors have been closed.
 * */
static void cleanFIFOs(int echo_fifo, int client_fifo) {
    // close the descriptors
    int ret = close(echo_fifo);
    ERROR_HELPER(ret, "Cannot close Echo FIFO");
    ret = close(client_fifo);
    ERROR_HELPER(ret, "Cannot close Client FIFO");

    // destroy the two FIFOs
    ret = unlink(ECHO_FIFO_NAME);
    ERROR_HELPER(ret, "Cannot unlink Echo FIFO");
    ret = unlink(CLNT_FIFO_NAME);
    ERROR_HELPER(ret, "Cannot unlink Client FIFO");
}

/** Echo component **/
int main(int argc, char* argv[]) {
    int ret;
    int echo_fifo, client_fifo;
    int bytes_left, bytes_sent, bytes_read;
    char buf[1024];

    char* quit_command = QUIT_COMMAND;
    size_t quit_command_len = strlen(quit_command);

    // Create the two FIFOs
    ret = mkfifo(ECHO_FIFO_NAME, 0666);
    ERROR_HELPER(ret, "Cannot create Echo FIFO");
    ret = mkfifo(CLNT_FIFO_NAME, 0666);
    ERROR_HELPER(ret, "Cannot create Client FIFO");

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
    echo_fifo = open(ECHO_FIFO_NAME, O_WRONLY);
    ERROR_HELPER(echo_fifo, "Cannot open Echo FIFO for writing");
    client_fifo = open(CLNT_FIFO_NAME, O_RDONLY);
    ERROR_HELPER(client_fifo, "Cannot open Client FIFO for reading");     

    // send welcome message
    sprintf(buf, "Hi! I'm an Echo process based on FIFOs.\nI will send you back through a FIFO whatever"
            " you send me through the other FIFO, and I will stop and exit when you send me %s.\n", quit_command);
    bytes_left = strlen(buf);
    bytes_sent = 0;
    /** [SOLUTION] SEND THE MESSAGE THROUGH THE ECHO FIFO
     *
     * Suggestions:
     * - you can write on the FIFO as on a regular file descriptor
     * - make sure that all the bytes have been written: use a while
     *   cycle in the implementation as we did for file descriptors!
     **/
    while (bytes_left > 0) {
        ret = write(echo_fifo, buf + bytes_sent, bytes_left);
        if (ret == -1 && errno == EINTR) continue;
        ERROR_HELPER(ret, "Cannot write to Echo FIFO");

        bytes_left -= ret;
        bytes_sent += ret;
    } 

    while (1) {
        /** [SOLUTION] READ THE MESSAGE THROUGH THE CLIENT FIFO
         *
         * Suggestions:
         * - you can read from a FIFO as from a regular file descriptor
         * - since you don't know the length of the message, just try
         *   to read the data available in the FIFO (and make sure that
         *   we have enough room to add a string terminator later)
         * - repeat the read() when interrupted before reading any data
         * - store the number of bytes read in 'bytes_read'
         * - reading 0 bytes means that the other process has closed
         *   the FIFO unexpectedly: this is an error to deal with!
         **/
        while ( (bytes_read = read(client_fifo, buf, sizeof(buf) - 1)) <= 0 ) {
            if (bytes_read == 0) {
                fprintf(stderr, "Client process has closed the Client FIFO unexpectedly! Exiting...\n");
                /* Error is likely to come from the client: thus we
                 * try to close server's descriptors and destroy the
                 * two FIFOs before actually exiting. */
                cleanFIFOs(echo_fifo, client_fifo);
                exit(EXIT_FAILURE);
            }
            if (errno == EINTR) continue;
            ERROR_HELPER(-1, "Cannot read from Client FIFO");
        }
        
        if (DEBUG) {
            buf[bytes_read] = '\0';
            printf("Message received: %s\n", buf);
        }

        // check whether I have just been told to quit...
        if (bytes_read == quit_command_len && !memcmp(buf, quit_command, quit_command_len)) break;

        // ... or if I have to send the message back through the Echo FIFO
        bytes_left = bytes_read;
        bytes_sent = 0;
        /** [SOLUTION] SEND THE MESSAGE THROUGH THE ECHO FIFO
         *
         * Suggestions:
         * - you can write on the FIFO as on a regular file descriptor
         * - make sure that all the bytes have been written: use a while
         *   cycle in the implementation as we did for file descriptors!
         **/
        while (bytes_left > 0) {
            ret = write(echo_fifo, buf + bytes_sent, bytes_left);
            if (ret == -1 && errno == EINTR) continue;
            ERROR_HELPER(ret, "Cannot write to Echo FIFO");

            bytes_left -= ret;
            bytes_sent += ret;
        }
    }

    // close the descriptors and destroy the two FIFOs
    cleanFIFOs(echo_fifo, client_fifo);

    exit(EXIT_SUCCESS);
}
