#ifndef IPC
#define IPC

#include <semaphore.h>

#define BUFFER_SIZE 1024
#define PIPE_NAME "pipe"

typedef struct {
    int socket_id;
    char message[BUFFER_SIZE];
} SharedMessage;

int shm_init(int *shm_fd, SharedMessage **shared_mem, sem_t **sem);
void shm_destroy(int shm_fd, SharedMessage *shared_mem, sem_t *sem);

int create_pipe();
void destroy_pipe();
int send_pipe_message(const char *message);
int read_pipe_message(char *buffer, size_t buffer_size);

struct SharedMessage {
    int socket_id;
    char message[BUFFER_SIZE];
};

#endif
