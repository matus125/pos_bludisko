#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include "game.h"

int server_fd = -1;
int server_running = 1;
SharedMemory *shared_mem;
sem_t *sem;
pthread_mutex_t client_lock = PTHREAD_MUTEX_INITIALIZER;

struct ClientInfo {
    int socket;
    pthread_t thread;
} clients[MAX_PLAYERS];

void save_results_to_file() {
    FILE *file = fopen("../data/results.txt", "a");
    if (!file) {
        perror("Nepodarilo sa otvorit subor na zapis.");
        return;
    }

    fprintf(file, "Hra skoncila. Vysledky:\n");
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (shared_mem->players[i].score > 0) {
            fprintf(file, "Hrac: %s, Umiestnenie: %d, Cas: %.2f sek.\n",shared_mem->players[i].name, shared_mem->players[i].score, shared_mem->players[i].time);
        }
    }
    fprintf(file, "\n");
    fclose(file);
}

void send_map_to_clients() {
    pthread_mutex_lock(&client_lock);
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (clients[i].socket != -1) {
            char map_buffer[BUFFER_SIZE] = {0};
            FILE *stream = fmemopen(map_buffer, BUFFER_SIZE, "w");
            if (stream) {
                print_maze(shared_mem, i, stream);
                fprintf(stream, "\n[System] Zadaj pohyb (w/a/s/d) alebo (e) pre odpojenie.\n");
                fclose(stream);
                send(clients[i].socket, map_buffer, strlen(map_buffer), 0);
            }
        }
    }
    pthread_mutex_unlock(&client_lock);
}

void broadcast_message(const char *message) {
    pthread_mutex_lock(&client_lock);
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (clients[i].socket != -1) {
            send(clients[i].socket, message, strlen(message), 0);
        }
    }
    pthread_mutex_unlock(&client_lock);
}

void reset_game() {
    shared_mem->num_players = 0;
    shared_mem->rank_counter = 1;

    pthread_mutex_lock(&client_lock);
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (clients[i].socket != -1) {
            close(clients[i].socket);
            clients[i].socket = -1;
        }

        if (clients[i].thread) {
            pthread_cancel(clients[i].thread);
            clients[i].thread = 0;
        }
    }
    pthread_mutex_unlock(&client_lock);

    for (int i = 0; i < MAX_PLAYERS; i++) {
        memset(&shared_mem->players[i], 0, sizeof(PlayerInfo));
    }

    memset(&shared_mem->game_state, 0, sizeof(GameState));
    generate_maze(&shared_mem->game_state);
}

void print_ranking() {
    char ranking_message[BUFFER_SIZE];
    int offset = snprintf(ranking_message, BUFFER_SIZE, "\n[System] Vsetci hraci dosiahli ciel. Toto su vysledky:\n");

    for (int i = 1; i <= MAX_PLAYERS; i++) {
        for (int j = 0; j < MAX_PLAYERS; j++) {
            if (shared_mem->players[j].score == i) {
                offset += snprintf(ranking_message + offset, BUFFER_SIZE - offset, "== Umiestnenie %d: %s s casom %.2f sek.\n", i, shared_mem->players[j].name, shared_mem->players[j].time);
            }
        }
    }

    strncat(ranking_message, "\n[System] Stlacte (e) pre vratenie na hlavne menu.\n", BUFFER_SIZE - offset - 1);

    save_results_to_file();
    broadcast_message(ranking_message);
    reset_game();
}

void init_player(PlayerInfo *player, int socket, const char *name) {
    player->socket_id = socket;
    snprintf(player->name, 50, "%s", name);
    player->pos_x = 1;
    player->pos_y = 1;
    player->score = 0;
    player->radius = 1;
    player->time = 0;
    player->start_time = time(NULL);
    shared_mem->num_players++;
}

void disconnect_player(int client_socket, int player_id) {
    sem_wait(sem);
    if (player_id != -1) {
        shared_mem->players[player_id].socket_id = 0;
        shared_mem->players[player_id].pos_x = 0;
        shared_mem->players[player_id].pos_y = 0;
        shared_mem->players[player_id].score = 0;
        shared_mem->players[player_id].radius = 0;
        shared_mem->num_players--;
    }
    sem_post(sem);

    pthread_mutex_lock(&client_lock);
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (clients[i].socket == client_socket) {
            close(clients[i].socket);
            clients[i].socket = -1;
            break;
        }
    }
    pthread_mutex_unlock(&client_lock);
}

void *client(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);

    char buffer[BUFFER_SIZE] = {0};
    recv(client_socket, buffer, BUFFER_SIZE, 0);
    buffer[strcspn(buffer, "\n")] = 0;

    int player_id = -1;
    sem_wait(sem);
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (shared_mem->players[i].socket_id == 0) {
            init_player(&shared_mem->players[i], client_socket, buffer);
            player_id = i;
            break;
        }
    }
    sem_post(sem);

    send_map_to_clients();

    while (1) {
        bzero(buffer, BUFFER_SIZE);
        int rec = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (rec <= 0 || strcmp(buffer, "exit") == 0) {
            disconnect_player(client_socket, player_id);
            pthread_exit(NULL);
        }

        sem_wait(sem);
        if (move_player(shared_mem, player_id, buffer[0])) {
            shared_mem->players[player_id].score = shared_mem->rank_counter++;
            snprintf(shared_mem->message, BUFFER_SIZE, "[System] Hrac %s dosiahol ciel!\n", shared_mem->players[player_id].name);
            broadcast_message(shared_mem->message);

            int finished_count = 0, active_players = 0;
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (clients[i].socket != -1) {
                    active_players++;
                    if (shared_mem->players[i].score > 0) finished_count++;
                }
            }
            if (finished_count == active_players && active_players > 0) {
                print_ranking();
            }
        }
        sem_post(sem);
        send_map_to_clients();
    }
}

int add_client(int socket) {
    pthread_mutex_lock(&client_lock);
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (clients[i].socket == -1) {
            clients[i].socket = socket;
            pthread_mutex_unlock(&client_lock);
            return i;
        }
    }
    pthread_mutex_unlock(&client_lock);
    return -1;
}

void *fifo_commands(void *arg) {
    char buffer[BUFFER_SIZE] = {0};
    while (server_running) {
        if (read_pipe_message(buffer, BUFFER_SIZE) == 0 && strcmp(buffer, "end") == 0) {
            printf("[System] Prijaty prikaz na zastavenie servera...\n");

            pthread_mutex_lock(&client_lock);
            int active_players = 0;
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (clients[i].socket != -1) {
                    active_players++;
                }
            }
            pthread_mutex_unlock(&client_lock);

            if (active_players > 0) {
                printf("[System] Server nie je mozne vypnut, pretoze su pripojeni hraci (%d).\n", active_players);
                continue;
            }

            server_running = 0;
            broadcast_message("[System] Server sa vypína...\n");

            pthread_mutex_lock(&client_lock);
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (clients[i].socket != -1) {
                    close(clients[i].socket);
                    clients[i].socket = -1;
                }
            }
            pthread_mutex_unlock(&client_lock);

            if (server_fd != -1) {
                shutdown(server_fd, SHUT_RDWR);
                close(server_fd);
                server_fd = -1;
            }
        }
        sleep(1);
    }
    return NULL;
}

void cleanup(pthread_t pipe_thread, int shm_fd) {
    pthread_join(pipe_thread, NULL);

    pthread_mutex_lock(&client_lock);
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (clients[i].socket != -1) {
            close(clients[i].socket);
            pthread_cancel(clients[i].thread);
            clients[i].socket = -1;
        }
    }
    pthread_mutex_unlock(&client_lock);

    destroy_pipe();
    shm_destroy(shm_fd, shared_mem, sem);
    if (server_fd != -1) {
        shutdown(server_fd, SHUT_RDWR);
        close(server_fd);
        server_fd = -1;
    }
    printf("[System] Server sa vypol.\n");
}


int main(int argc, char *argv[]) {
    srand(time(NULL));
    int shm_fd;

    if (argc != 2) {
        fprintf(stderr, "[Error] Pouzitie: %s <port>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    if (port <= 2500 || port > 9999) {
        fprintf(stderr, "[Error] Port musi byt cislo medzi 2500 a 9999.\n");
        return 1;
    }

    if (shm_init(&shm_fd, &shared_mem, &sem) < 0 || create_pipe() < 0) {
        return 1;
    }

    sem_wait(sem);
    shared_mem->rank_counter = 1;
    generate_maze(&shared_mem->game_state);
    sem_post(sem);

    printf("[System] Bludisko bolo uspesne vygenerované.\n");

    pthread_t pipe_thread;
    pthread_create(&pipe_thread, NULL, fifo_commands, NULL);

    for (int i = 0; i < MAX_PLAYERS; i++) {
        clients[i].socket = -1;
    }

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("[Error] Chyba pri vytvarani servera");
        cleanup(pipe_thread, shm_fd);
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in serverAddr;
    memset(&serverAddr, '\0', sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("[Error] Chyba pri bind.");
        cleanup(pipe_thread, shm_fd);
        return 1;
    }

    if (listen(server_fd, MAX_PLAYERS) < 0) {
        perror("[Error] Chyba pri listen.");
        cleanup(pipe_thread, shm_fd);
        return 1;
    }

    printf("[System] Server pocuva na porte %d...\n", port);

    while (server_running) {
        int *client_socket = malloc(sizeof(int));
        socklen_t addr_size = sizeof(struct sockaddr_in);
        *client_socket = accept(server_fd, NULL, &addr_size);

        if (*client_socket < 0) {
            free(client_socket);
            if (!server_running) break;
            perror("[Error] Chyba pri prijati klienta");
            continue;
        }

        if (add_client(*client_socket) == -1) {
            send(*client_socket, "[System] Server je plny. Skuste neskor.\n", 40, 0);
            close(*client_socket);
            free(client_socket);
        } else {
            pthread_t client_thread;
            pthread_create(&client_thread, NULL, client, client_socket);
            pthread_detach(client_thread);
        }
    }

    cleanup(pipe_thread, shm_fd);
    return 0;
}