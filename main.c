#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>

#define DATA_SIZE (256) // 16 MB

#define FIFO_WRITE "/dev/axis_fifo_0x0000000080030000"
#define FIFO_READ  "/dev/axis_fifo_0x0000000080060000"

int main(int argc, char *argv[]) {
    char *buffer_out = malloc(DATA_SIZE);
    char *buffer_in = malloc(DATA_SIZE);

    char *fifo_in = FIFO_READ;
    char *fifo_out = FIFO_WRITE;

    if (argc >= 2) {
        fifo_out = argv[1];
    }
    if (argc >= 3) {
        fifo_in = argv[2];
    }

    if (!buffer_out || !buffer_in) {
        perror("Failed to allocate memory");
        return EXIT_FAILURE;
    }

    // Initialize buffer with some data
    for (unsigned i=0; i<DATA_SIZE; i++) {
        buffer_out[i] = (0xCAFEDECA ^ i) + i;
    }

    printf("Opening FIFOs: %s -> %s \n", fifo_out, fifo_in);

    // Note: Opening a FIFO for writing will block until a reader opens it.
    int fd_write = open(fifo_out, O_WRONLY);
    if (fd_write == -1) {
        perror("Error opening write FIFO");
        return EXIT_FAILURE;
    }

    int fd_read = open(fifo_in, O_RDONLY);
    if (fd_read == -1) {
        perror("Error opening read FIFO");
        close(fd_write);
        return EXIT_FAILURE;
    }

    // 1. Write 16MB to the FIFO
    printf("Writing 16MB to %s...\n", fifo_out);
    ssize_t bytes_written = write(fd_write, buffer_out, DATA_SIZE);
    if (bytes_written == -1) {
        perror("Write failed");
    } else {
        printf("Successfully wrote %zd bytes.\n", bytes_written);
    }

    // 2. Read 16MB from the other FIFO
    printf("Reading 16MB from %s...\n", fifo_in);
    ssize_t total_read = 0;
    while (total_read < DATA_SIZE) {
        ssize_t bytes_read = read(fd_read, buffer_in + total_read, DATA_SIZE - total_read);
        if (bytes_read <= 0) {
            printf("Error while reading\n");
            goto finish;
        }
        total_read += bytes_read;
    }

    int diff = memcmp(buffer_out, buffer_in, DATA_SIZE);
    printf("Successfully read %zd bytes, diff = %d.\n", total_read, diff);

finish:
    // Cleanup
    close(fd_write);
    close(fd_read);
    free(buffer_out);
    free(buffer_in);

    return EXIT_SUCCESS;
}
