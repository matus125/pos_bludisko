#ifndef TYPES
#define TYPES

#define MAZE_SIZE 7
#define BUFFER_SIZE 1024
#define MAX_PLAYERS 5

typedef struct {
    int maze[MAZE_SIZE][MAZE_SIZE];
    int dest_x;
    int dest_y;
} GameState;

typedef struct {
    int socket_id;
    char name[50];
    int pos_x;
    int pos_y;
    int score;
    double time;
} PlayerInfo;

#endif
