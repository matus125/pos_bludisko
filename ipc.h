#ifndef IPC
#define IPC

#include <semaphore.h>
#include "types.h"

#define PIPE_NAME "pipe"

typedef struct {
    PlayerInfo players[MAX_PLAYERS];
    int num_players;
    int rank_counter;
    char message[BUFFER_SIZE];
    GameState game_state;
} SharedMemory;

int shm_init(int *shm_fd, SharedMemory **shared_mem, sem_t **sem);
void shm_destroy(int shm_fd, SharedMemory *shared_mem, sem_t *sem);

int create_pipe();
void destroy_pipe();
int send_pipe_message(const char *message);
int read_pipe_message(char *buffer, size_t buffer_size);

#endif
