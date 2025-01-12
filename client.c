#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <pthread.h>
#include "ipc.h"

int connection_lost = 0;

int connect_to_server(int port) {
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        perror("[Error] Chyba pri vytvarani socketu.");
        return -1;
    }

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(clientSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("[Error] Chyba pri pripojovani na server.");
        close(clientSocket);
        return -1;
    }

    return clientSocket;
}

void start_server(int port) {
    int clientSocket = connect_to_server(port);

    if (clientSocket >= 0) {
        printf("[System] Pripajam sa na existujuci server...\n");
        close(clientSocket);
        return;
    }

    printf("[Klient] Spustam novy server na porte %d...\n", port);

    pid_t pid = fork();
    if (pid == 0) {
        char port_str[6];
        snprintf(port_str, sizeof(port_str), "%d", port);
        // execlp("valgrind", "valgrind", "--leak-check=full", "--show-leak-kinds=all", "--track-origins=yes", "./server", port_str, NULL);
        execlp("./server", "./server", port_str, NULL);
        perror("[Error] Chyba pri spusteni servera.");
        exit(1);
    } else if (pid < 0) {
        perror("[Error] Chyba pri forku.");
        exit(1);
    }
    sleep(1);

    printf("[System] Server uspesne spusteny.\n");
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

        if (rec <= 0 || strstr(buffer, "[System] Odpajam...") != NULL) {
            printf("\n[System] Spojenie so serverom bolo prerusene.\n");
            pthread_exit(NULL);
        }

        printf("\n%s\n", buffer);
    }

    return NULL;
}

void handle_game(int clientSocket) {
    pthread_t server_thread;
    pthread_create(&server_thread, NULL, listen_to_server, &clientSocket);

    system("stty cbreak -echo");
    while (!connection_lost) {
        char input = getchar();
        if (input == 'e') {
            printf("\n[Klient] Odpojil som sa zo serveru.\n");
            break;
        }
        send(clientSocket, &input, 1, 0);
    }
    system("stty sane");

    pthread_cancel(server_thread);
    pthread_join(server_thread, NULL);
    close(clientSocket);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "[Error] Pouzitie: %s <port>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    if (port <= 2500 || port > 9999) {
        fprintf(stderr, "[Error] Port musi byt cislo vacsie ako 2500 a 4-ciferne.\n");
        return 1;
    }

    int choice;
    char buffer[BUFFER_SIZE];
    while (1) {
        display_menu();
        scanf("%d", &choice);
        getchar();

        switch (choice) {
            case 1:
                printf("[Klient] Spustam server na porte %d...\n", port);
                start_server(port);
                break;
            case 2: {
                printf("[Klient] Pripajam sa k serveru na porte %d...\n", port);
                int clientSocket = connect_to_server(port);
                if (clientSocket < 0) break;

                printf("[System] Zadajte svoje meno: ");
                fgets(buffer, BUFFER_SIZE, stdin);
                buffer[strcspn(buffer, "\n")] = 0;
                send(clientSocket, buffer, strlen(buffer), 0);

                printf("[Klient] Pripojil som sa k serveru.\n");
                handle_game(clientSocket);
                break;
            }
            case 3:
                printf("[Klient] Odosielam prikaz na vypnutie servera...\n");
                if (send_pipe_message("end") < 0) {
                    fprintf(stderr, "[Error] Ziaden server nie je zapnuty.\n");
                }
                break;

            case 4:
                printf("[Klient] Zobrazenie histórie výsledkov...\n");
                FILE *file = fopen("../data/results.txt", "r");
                if (!file) {
                    perror("[Error] Nepodarilo sa otvoriť súbor s výsledkami.");
                    break;
                }

                char line[BUFFER_SIZE];
                printf("\n===== História výsledkov =====\n");
                while (fgets(line, BUFFER_SIZE, file) != NULL) {
                    printf("%s", line);
                }
                printf("================================\n");
                fclose(file);
                break;

            case 5:
                printf("[Klient] Vypinam.\n");
                exit(0);    
                break;

            default:
                printf("[Error] Neplatna volba. Skuste znova.\n");
                break;
        }
    }

    return 0;
}
