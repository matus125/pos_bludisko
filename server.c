#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include "ipc.h"

#define MAX_CLIENTS 2

int server_fd = -1;
int server_running = 1;
SharedMessage *shared_mem;
sem_t *sem;
pthread_mutex_t client_lock = PTHREAD_MUTEX_INITIALIZER;

struct ClientInfo {
    int socket;
    pthread_t thread;
} clients[MAX_CLIENTS];

void *client(void *arg) {
    int client_socket = *(int *)arg;
    char buffer[BUFFER_SIZE];

    while (1) {
        bzero(buffer, BUFFER_SIZE);
        int rec = recv(client_socket, buffer, BUFFER_SIZE, 0);

        if (rec <= 0 || strcmp(buffer, ":exit") == 0) {
            printf("[System] Klient na sockete %d sa odpojil.\n", client_socket);
            break;
        }

        sem_wait(sem);
        shared_mem->socket_id = client_socket;
        strncpy(shared_mem->message, buffer, BUFFER_SIZE);
        sem_post(sem);

        pthread_mutex_lock(&client_lock);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].socket != -1) {
                char send_buffer[BUFFER_SIZE];
                snprintf(send_buffer, BUFFER_SIZE, "[System] Klient %d poslal: %s", shared_mem->socket_id, shared_mem->message);
                send(clients[i].socket, send_buffer, strlen(send_buffer), 0);
            }
        }
        pthread_mutex_unlock(&client_lock);
    }

    close(client_socket);

    pthread_mutex_lock(&client_lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket == client_socket) {
            clients[i].socket = -1;
            break;
        }
    }
    pthread_mutex_unlock(&client_lock);

    return NULL;
}

void *fifo_commands(void *arg) {
    char buffer[BUFFER_SIZE];

    while (server_running) {
        if (read_pipe_message(buffer, BUFFER_SIZE) == 0) {
            if (strcmp(buffer, "end") == 0) {
                printf("[System] Zastavujem server...\n");
                server_running = 0;
                shutdown(server_fd, SHUT_RDWR);
                break;
            }
        }
        sleep(1);
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "[Error] Pouzitie: %s <port>\n", argv[0]);
        exit(1);
    }

    int port = atoi(argv[1]);
    if (port <= 2500 || port > 9999) {
        fprintf(stderr, "[Error] Port musi byt cislo vacsie ako 2500 a 4-ciferne.\n");
        exit(1);
    }

    int shm_fd;
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t addr_size;

    if (shm_init(&shm_fd, &shared_mem, &sem) < 0) {
        exit(1);
    }

    if (create_pipe() < 0) {
        exit(1);
    }

    pthread_t pipe_thread;
    pthread_create(&pipe_thread, NULL, fifo_commands, NULL);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].socket = -1;
    }

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Chyba pri vytvarani servera.");
        exit(1);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&serverAddr, '\0', sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Chyba pri bind.");
        close(server_fd);
        exit(1);
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("Chyba pri listen");
        close(server_fd);
        exit(1);
    }
    printf("[System] Server pocuva na porte %d...\n", port);

    while (server_running) {
        addr_size = sizeof(clientAddr);
        int *client_socket = malloc(sizeof(int));

        *client_socket = accept(server_fd, (struct sockaddr *)&clientAddr, &addr_size);
        if (*client_socket < 0) {
            if (!server_running) {
                free(client_socket);
                break;
            }
            perror("Chyba");
            free(client_socket);
            continue;
        }

        printf("[System] Pripojenie z %s:%d\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

        pthread_mutex_lock(&client_lock);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].socket == -1) {
                clients[i].socket = *client_socket;
                pthread_create(&clients[i].thread, NULL, client, client_socket);
                pthread_detach(clients[i].thread);
                break;
            }
        }
        pthread_mutex_unlock(&client_lock);
    }

    pthread_join(pipe_thread, NULL);
    destroy_pipe();

    pthread_mutex_lock(&client_lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != -1) {
            close(clients[i].socket);
        }
    }
    pthread_mutex_unlock(&client_lock);
    shm_destroy(shm_fd, shared_mem, sem);
    close(server_fd);
    printf("[System] Server sa vypol.\n");
    return 0;
}
