#include <stdbool.h>
#ifndef GAME
#define GAME
#define MAZE_WIDTH 21
#define MAZE_HEIGHT 21

struct GameState {
  int maze[MAZE_WIDTH][MAZE_HEIGHT];
  int pos_x;
  int pos_y;
  int dest_x;
  int dest_y;
};

void generate_maze(struct GameState *state);
void dfs_algorithm(int maze[MAZE_HEIGHT][MAZE_WIDTH], int x, int y);
void shuffle(int directions[4][2], int n);
void print_maze(struct GameState *state);

bool can_move_there(struct GameState *state, int x, int y);
void move_player(struct GameState *state, char direction);

#endif // GAME
