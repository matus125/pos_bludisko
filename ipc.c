#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <stddef.h>
#include "ipc.h"

#define SHM_NAME "/my_shared_memory"
#define SEM_NAME "/my_semaphore"

int shm_init(int *shm_fd, SharedMessage **shared_mem, sem_t **sem) {
    *shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (*shm_fd < 0) {
        perror("Chyba pri vytvarani zdielanej pamäte.");
        return -1;
    }

    if (ftruncate(*shm_fd, sizeof(SharedMessage)) < 0) {
        perror("Chyba pri nastavovani velkosti zdielanej pamäte.");
        shm_unlink(SHM_NAME);
        return -1;
    }

    *shared_mem = mmap(NULL, sizeof(SharedMessage), PROT_READ | PROT_WRITE, MAP_SHARED, *shm_fd, 0);
    if (*shared_mem == MAP_FAILED) {
        perror("Chyba pri mapovani zdielanej pamäte.");
        shm_unlink(SHM_NAME);
        return -1;
    }

    *sem = sem_open(SEM_NAME, O_CREAT, 0666, 1);
    if (*sem == SEM_FAILED) {
        perror("Chyba pri vytvarani semaforu.");
        munmap(*shared_mem, sizeof(SharedMessage));
        shm_unlink(SHM_NAME);
        return -1;
    }

    return 0;
}

void shm_destroy(int shm_fd, SharedMessage *shared_mem, sem_t *sem) {
    munmap(shared_mem, sizeof(SharedMessage));
    close(shm_fd);
    shm_unlink(SHM_NAME);

    sem_close(sem);
    sem_unlink(SEM_NAME);
}

int create_pipe() {
    if (mkfifo(PIPE_NAME, 0666) < 0 && errno != EEXIST) {
        perror("Chyba pri vytvarani pipe.");
        return -1;
    }
    
    return 0;
}

void destroy_pipe() {
    unlink(PIPE_NAME);
}

int send_pipe_message(const char *message) {
    if (message == NULL) {
        perror("Zla sprava.");
        return -1;
    }

    int pipe_fd = open(PIPE_NAME, O_WRONLY);
    if (pipe_fd < 0) {
        perror("Chyba pri otvarani pipe.");
        return -1;
    }

    if (write(pipe_fd, message, strlen(message)) < 0) {
        perror("Chyba pri vpisovani do pipe.");
        close(pipe_fd);
        return -1;
    }

    close(pipe_fd);

    return 0;
}

int read_pipe_message(char *buffer, size_t buffer_size) {
    if (buffer == NULL || buffer_size == 0) {
        perror("Chyba s bufferom.");
        return -1;
    }

    int pipe_fd = open(PIPE_NAME, O_RDONLY);
    if (pipe_fd < 0) {
        return -1;
    }

    ssize_t rec = read(pipe_fd, buffer, buffer_size - 1);
    if (rec < 0) {
        perror("Chyba pri citani z pipe.");
        close(pipe_fd);
        return -1;
    }

    buffer[rec] = '\0';
    close(pipe_fd);

    return 0;
}
