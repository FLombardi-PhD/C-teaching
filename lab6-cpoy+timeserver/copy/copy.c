#include <errno.h>
#include <fcntl.h> // macros for open (e.g., O_RDONLY, O_WRONLY)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define GENERIC_ERROR_HELPER(cond, errCode, msg) do {               \
        if (cond) {                                                 \
            fprintf(stderr, "%s: %s\n", msg, strerror(errCode));    \
            exit(EXIT_FAILURE);                                     \
        }                                                           \
    } while(0)

#define ERROR_HELPER(ret, msg)      GENERIC_ERROR_HELPER((ret < 0), errno, msg)

#define DEFAULT_BLOCK_SIZE  128;

static inline void performCopyBetweenDescriptors(int src_fd, int dest_fd, int block_size) {
    char* buf = malloc(block_size);

    while (1) {
        int read_bytes = 0; // index for writing into the buffer
        int bytes_left = block_size; // number of bytes to (possibly) read

        while (bytes_left > 0) {
            /** [SOLUTION]
             *
             * Suggestion: when there are no more data to read, read()
             * will return 0; insert a break and exit the loop!
             *
             * Note that a read() request can be interrupted by a
             * signal and two outcomes are possible:
             * a) if zero bytes have been read, it will return -1 and
             *    errno will be set to EINTR: you will have to repeat
             *    the read() operation
             * b) if X<N bytes have been read, it will return X and
             *    you have to read N-X bytes in the next iteration
             *
             * In a correct solution you have to deal explicitly with
             * the two cases described above. */
            int ret = read(src_fd, buf + read_bytes, bytes_left);

            // no more bytes left to read!
            if (ret == 0) break;

            // read() was interrupted by a signal before it read any data
            if (ret == -1 && errno == EINTR) continue;

            // handle generic errors
            ERROR_HELPER(ret, "Cannot read from source file");

            /* The value returned may be less than bytes_left if the number
             * of bytes left in the file is less than bytes_left, if the
             * read() request was interrupted by a signal, or if the file
             * is a pipe or FIFO or special file and has fewer than
             * bytes_left bytes immediately available for reading */
            bytes_left -= ret;
            read_bytes += ret;
        }

        // no more bytes left to write!
        if (read_bytes == 0) break;

        int written_bytes = 0; // index for reading from the buffer
        bytes_left = read_bytes; // number of bytes to write

        while (bytes_left > 0) {
            /** [SOLUTION]
             *
             * Suggestion: in the write() case you won't have to check
             * if the return value is 0 as you did for the read()
             *
             * Again, note that a write() request can be interrupted by
             * a signal, and two outcomes are possible:
             * a) if zero bytes have been written, it will return -1
             *    and errno will be set to EINTR: you will have to
             *    repeat the write() operation
             * b) if X<N bytes have been written, it will return X and
             *    you have to write N-X bytes in the next iteration
             *
             * In a correct solution you have to deal explicitly with
             * the two cases described above. */
            int ret = write(dest_fd, buf + written_bytes, bytes_left);

            // write() was interrupted by a signal before it wrote any data
            if (ret == -1 && errno == EINTR) continue;

            // handle generic errors
            FD_ERROR_HELPER(ret, "Cannot write to destination file");

            bytes_left -= ret;
            written_bytes += ret;
        }
    }

    free(buf);
}

int main(int argc, char* argv[]) {
    int block_size, src_fd, dest_fd;

    if (argc == 4) {
        block_size = atoi(argv[3]);
    } else {
        block_size = DEFAULT_BLOCK_SIZE;
    }

    if (argc < 3 || argc > 4 || block_size <= 0) {
        fprintf(stderr, "Syntax: %s <source_file> <dest_file> [<block_size>]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // create descriptors for source and destination files
    src_fd = open(argv[1], O_RDONLY);
    ERROR_HELPER(src_fd, "Could not open source file");

    // for simplicity we use rw-r--r-- permissions for the destination file
    dest_fd = open(argv[2], O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (dest_fd < 0 && errno == EEXIST) {
        fprintf(stderr, "WARNING: file %s already exists, I will overwrite it!\n", argv[2]);
        dest_fd = open(argv[2], O_WRONLY | O_CREAT, 0644);
    }
    ERROR_HELPER(dest_fd, "Could not create destination file");

    // use a helper method to actually perform the copy
    performCopyBetweenDescriptors(src_fd, dest_fd, block_size);

    // close the descriptors
    int ret = close(src_fd);
    ERROR_HELPER(ret, "Could not close source file");
    ret = close(dest_fd);
    ERROR_HELPER(ret, "Could not close destination file");

    exit(EXIT_SUCCESS);
}
