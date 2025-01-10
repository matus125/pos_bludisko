#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <pthread.h>
#include "ipc.h"

int connect_to_server(int port) {
    int clientSocket;
    struct sockaddr_in serverAddr;

    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        perror("Chyba pri vytvarani socketu.");
        return -1;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(clientSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        close(clientSocket);
        return -1;
    }

    return clientSocket;
}

void start_server(int port) {
    pid_t pid = fork();

    if (pid == 0) {
        char port_str[6];
        snprintf(port_str, sizeof(port_str), "%d", port);
        execl("./server", "./server", port_str, NULL);
        perror("Chyba pri starte servera.");
        exit(1);
    } else if (pid > 0) {
        sleep(1);
    } else {
        perror("Chyba pri forku.");
        exit(1);
    }
}

void display_menu() {
    printf("\n===== Menu =====\n");
    printf("1. Start server (hra)\n");
    printf("2. Pripojit sa k serveru\n");
    printf("3. Vypnut server\n");
    printf("4. Historia\n");
    printf("5. Exit\n");
    printf("================\n");
    printf("Vyberte moznost: ");
}

void *listen_to_server(void *arg) {
    int clientSocket = *(int *)arg;
    char buffer[BUFFER_SIZE];

    while (1) {
        bzero(buffer, BUFFER_SIZE);
        int rec = recv(clientSocket, buffer, BUFFER_SIZE, 0);

        if (rec <= 0) {
            printf("\n[System]: Spojenie so serverom bolo prerušené.\n");
            pthread_exit(NULL);
        }

        printf("\n%s\n", buffer);
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    char buffer[BUFFER_SIZE];
    int clientSocket, port, choice;
    int shm_fd;
    SharedMessage *shared_mem;
    sem_t *sem;

    if (argc != 2) {
        fprintf(stderr, "[Error] Pouzitie: <port>\n");
        exit(1);
    }

    port = atoi(argv[1]);
    if (port <= 2500 || port > 9999) {
        fprintf(stderr, "[Error] ]Port musi byt cislo vacsie ako 2000 a 4-ciferne.\n");
        exit(1);
    }

    if (shm_init(&shm_fd, &shared_mem, &sem) < 0) {
        exit(1);
    }

    while (1) {
        display_menu();
        scanf("%d", &choice);
        getchar();

        if (choice == 1) {
            printf("[Klient] Spustam server na porte %d...\n", port);
            start_server(port);

            clientSocket = connect_to_server(port);
            if (clientSocket < 0) {
                fprintf(stderr, "[Error] Chyba pri pripojovani na server.\n");
                continue;
            }

            printf("[Klient] Pripojil som sa k serveru.\n");
            break;
        } else if (choice == 2) {
            printf("[Klient] Pripajam sa k existujúcemu serveru na porte %d...\n", port);

            clientSocket = connect_to_server(port);
            if (clientSocket < 0) {
                fprintf(stderr, "[Error] Server nie je dostupný.\n");
                continue;
            }

            printf("[Klient] Pripojil som sa k serveru.\n");
            break;
        } else if (choice == 3) {
            printf("[Klient] Odosielam prikaz na vypnutie servera...\n");
            if (send_pipe_message("end") < 0) {
                fprintf(stderr, "[Error] Ziaden server neni zapnuty.\n");
            }
        } else if (choice == 5) {
            printf("[Klient] Vypinam.\n");
            shm_destroy(shm_fd, shared_mem, sem);
            exit(0);
        } else {
            printf("[Error] Neplatna volba. Skuste znova.\n");
        }
    }

    pthread_t server_thread;
    pthread_create(&server_thread, NULL, listen_to_server, &clientSocket);

    while (1) {
        printf("Klient: \n");
        fgets(buffer, BUFFER_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = 0;

        send(clientSocket, buffer, strlen(buffer), 0);

        if (strcmp(buffer, ":exit") == 0) {
            printf("[System] Klient sa odpojil so serveru.\n");
            close(clientSocket);
            pthread_cancel(server_thread);
            break;
        }
    }

    pthread_join(server_thread, NULL);
    shm_destroy(shm_fd, shared_mem, sem);

    return 0;
}
