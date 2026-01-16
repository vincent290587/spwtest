#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

#define DATA_SIZE  (16 * 1024 * 1024) // 16 MB
#define CHUNK_SIZE (512)              // Maximum bytes per write call
#define FIFO_WRITE "/dev/axis_fifo_0x0000000080090000"
#define FIFO_READ  "/dev/axis_fifo_0x0000000080090000"

typedef struct {
    int fd;
    void *buffer;
    size_t size;
    const char *name;
} thread_args_t;

void* write_thread_func(void *arg) {
    thread_args_t *args = (thread_args_t *)arg;
    ssize_t total_written = 0;

    printf("[Writer] Starting transfer to %s in %d-byte chunks...\n", args->name, CHUNK_SIZE);

    while (total_written < args->size) {
        // Calculate remaining bytes, capped at CHUNK_SIZE
        size_t remaining = args->size - total_written;
        size_t to_write = (remaining > CHUNK_SIZE) ? CHUNK_SIZE : remaining;

        ssize_t bytes = write(args->fd, (unsigned char*)args->buffer + total_written, to_write);

        if (bytes <= 0) {
            if (errno == EINTR) continue;
            perror("[Writer] Write failed");
            break;
        }
        total_written += bytes;
    }

    printf("[Writer] Finished writing %zd bytes.\n", total_written);
    return NULL;
}

int main(int argc, char *argv[]) {
    pthread_t writer_thread;
    char *fifo_out = (argc >= 2) ? argv[1] : FIFO_WRITE;
    char *fifo_in  = (argc >= 3) ? argv[2] : FIFO_READ;

    unsigned char *buffer_out = malloc(DATA_SIZE);
    unsigned char *buffer_in  = malloc(DATA_SIZE);

    if (!buffer_out || !buffer_in) {
        perror("Failed to allocate memory");
        return EXIT_FAILURE;
    }

    // Initialize buffer with the requested pattern
    for (unsigned i = 0; i < DATA_SIZE; i++) {
        buffer_out[i] = (unsigned char)((0xCAFEDECA ^ i) + i);
    }

    printf("Opening FIFOs: %s (W) and %s (R)\n", fifo_out, fifo_in);

    // Opening O_WRONLY blocks until a reader is ready
    int fd_write = open(fifo_out, O_WRONLY);
    if (fd_write == -1) { perror("Error opening write FIFO"); return 1; }

    int fd_read = open(fifo_in, O_RDONLY);
    if (fd_read == -1) { perror("Error opening read FIFO"); close(fd_write); return 1; }

    thread_args_t w_args = { .fd = fd_write, .buffer = buffer_out, .size = DATA_SIZE, .name = fifo_out };

    if (pthread_create(&writer_thread, NULL, write_thread_func, &w_args) != 0) {
        perror("Failed to create writer thread");
        return 1;
    }

    // Reader logic (Main Thread)
    printf("[Reader] Starting read from %s...\n", fifo_in);
    ssize_t total_read = 0;
    while (total_read < DATA_SIZE) {
        ssize_t bytes_read = read(fd_read, buffer_in + total_read, DATA_SIZE - total_read);
        if (bytes_read <= 0) {
            if (errno == EINTR) continue;
            printf("[Reader] Error or EOF while reading\n");
            break;
        }
        total_read += bytes_read;
    }

    pthread_join(writer_thread, NULL);

    // Verification
    int diff = memcmp(buffer_out, buffer_in, DATA_SIZE);
    printf("\n--- Result ---\n");
    printf("Read %zd/%d bytes. Verification: %s\n",
            total_read, DATA_SIZE, (diff == 0) ? "PASS" : "FAIL");

    if (diff != 0) {
        for(size_t i = 0; i < DATA_SIZE; i++) {
            if(buffer_out[i] != buffer_in[i]) {
                printf("Mismatch at byte %zu: Sent 0x%02X, Recv 0x%02X\n", i, buffer_out[i], buffer_in[i]);
                break;
            }
        }
    }

    close(fd_write);
    close(fd_read);
    free(buffer_out);
    free(buffer_in);

    return (diff == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}