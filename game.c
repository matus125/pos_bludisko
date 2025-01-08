#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "game.h"

void generate_maze(struct GameState *state) {
  for (int i = 0; i < MAZE_HEIGHT; i++) {
    for (int j = 0; j < MAZE_WIDTH; j++) {
      state->maze[i][j] = 1;
    }
  }

  dfs_algorithm(state->maze, 1, 1);
  
  state->pos_x = 1;
  state->pos_y = 1;
  
  state->dest_x = MAZE_WIDTH - 2;
  state->dest_y = MAZE_HEIGHT - 2;
}

void dfs_algorithm(int maze[MAZE_HEIGHT][MAZE_WIDTH], int x, int y) {
  maze[y][x] = 0;

  int directions[4][2] = {
    {0, -1},
    {0, 1},
    {-1, 0},
    {1, 0},
  };

  shuffle(directions, 4);

  for (int i = 0; i < 4; i++) {
    int nx = x + 2 * directions[i][0];
    int ny = y + 2 * directions[i][1];

    if (
      nx > 0 && nx < MAZE_WIDTH - 1 && 
      ny > 0 && ny < MAZE_HEIGHT - 1 && 
      maze[ny][nx] == 1
    ) {
      maze[y + directions[i][1]][x + directions[i][0]] = 0;
      dfs_algorithm(maze, nx, ny);
    }
  }

}

void shuffle(int directions[4][2], int n) {
  for (int i = n - 1; i > 0; i--) {
    int j = rand() % (i + 1);

    int temp_x = directions[i][0];
    int temp_y = directions[i][1];

    directions[i][0] = directions[j][0];
    directions[i][1] = directions[j][1];

    directions[j][0] = temp_x;
    directions[j][1] = temp_y;
  }
}

void print_maze(struct GameState *state) {
  system("clear");

  for (int i = 0; i < MAZE_HEIGHT; i++) {
    for (int j = 0; j < MAZE_WIDTH; j++) {
      if (i == state->pos_y && j == state->pos_x) {
        printf("P "); // Hrac
      } else if (i == state->dest_y && j == state->dest_x) {
        printf("D "); // Ciel
      } else if (state->maze[i][j] == 1) {
        printf("# "); // Stena
      } else {
        printf("  "); // Chodba
      }
    }
    printf("\n");
  }
}

bool can_move_there(struct GameState *state, int x, int y) {
  return ((x >= 0 && x < MAZE_WIDTH) && (y >= 0 && y < MAZE_HEIGHT) && state->maze[y][x] == 0);
}

void move_player(struct GameState *state, char direction) {
  int x = state->pos_x;
  int y = state->pos_y;

  switch (direction) {
    case 'w': // Hore
      y--;
      break;
    case 'a': // Dolava
      x--;
      break;
    case 's': // Dole
      y++;
      break;
    case 'd': // Doprava
      x++;
      break;
    default:
      printf("Pohyb moze byt len hore (w), dole (s), dolava (a) a doprava (d).\n");
      return;
  }

  if (can_move_there(state, x, y)) {
    state->pos_x = x;
    state->pos_y = y;
  };

  print_maze(state);
}
