#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>

#define DATA_SIZE  (16 * 1024 * 1024) // 16 MB
#define CHUNK_SIZE (512)              // Max bytes per write
#define FIFO_WRITE "/dev/axis_fifo_0x0000000080090000"
#define FIFO_READ  "/dev/axis_fifo_0x0000000080090000"

typedef struct {
    int fd;
    void *buffer;
    size_t size;
} thread_args_t;

void* write_thread_func(void *arg) {
    thread_args_t *args = (thread_args_t *)arg;
    size_t total_written = 0;
    while (total_written < args->size) {
        size_t to_write = (args->size - total_written > CHUNK_SIZE) ? CHUNK_SIZE : (args->size - total_written);
        ssize_t bytes = write(args->fd, (unsigned char*)args->buffer + total_written, to_write);
        if (bytes <= 0) { if (errno == EINTR) continue; break; }
        total_written += bytes;
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    pthread_t writer_thread;
    char *fifo_out = (argc >= 2) ? argv[1] : FIFO_WRITE;
    char *fifo_in  = (argc >= 3) ? argv[2] : FIFO_READ;
    struct timespec start, end;

    unsigned char *buffer_out = malloc(DATA_SIZE);
    unsigned char *buffer_in  = malloc(DATA_SIZE);
    for (unsigned i = 0; i < DATA_SIZE; i++) buffer_out[i] = (unsigned char)((0xCAFEDECA ^ i) + i);

    int fd_write = open(fifo_out, O_WRONLY);
    int fd_read = open(fifo_in, O_RDONLY);
    if (fd_write == -1 || fd_read == -1) { perror("Open failed"); return 1; }

    thread_args_t w_args = { .fd = fd_write, .buffer = buffer_out, .size = DATA_SIZE };

    // --- Start Timing ---
    clock_gettime(CLOCK_MONOTONIC, &start);

    pthread_create(&writer_thread, NULL, write_thread_func, &w_args);

    size_t total_read = 0;
    while (total_read < DATA_SIZE) {
        ssize_t bytes_read = read(fd_read, buffer_in + total_read, DATA_SIZE - total_read);
        if (bytes_read <= 0) { if (errno == EINTR) continue; break; }
        total_read += bytes_read;
    }

    pthread_join(writer_thread, NULL);

    // --- Stop Timing ---
    clock_gettime(CLOCK_MONOTONIC, &end);

    // Calculate elapsed time in seconds
    double time_spent = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1000000000.0;
    double mb_per_sec = (DATA_SIZE / (1024.0 * 1024.0)) / time_spent;

    printf("\n--- Performance Metrics ---\n");
    printf("Time taken: %.4f seconds\n", time_spent);
    printf("Throughput: %.2f MB/s\n", mb_per_sec);
    printf("Verification: %s\n", (memcmp(buffer_out, buffer_in, DATA_SIZE) == 0) ? "PASS" : "FAIL");

    close(fd_write); close(fd_read);
    free(buffer_out); free(buffer_in);
    return 0;
}