#ifndef TYPES
#define TYPES

#define MAZE_SIZE 21
#define BUFFER_SIZE 1024
#define MAX_PLAYERS 2

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
    int radius;
    int score;
    double time;
    double start_time;
} PlayerInfo;

#endif
