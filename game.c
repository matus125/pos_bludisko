#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "game.h"

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

void dfs_algorithm(int maze[MAZE_SIZE][MAZE_SIZE], int x, int y) {
    maze[y][x] = 0;

    int directions[4][2] = {
        {0, -1}, {0, 1}, {-1, 0}, {1, 0},
    };

    shuffle(directions, 4);

    for (int i = 0; i < 4; i++) {
        int nx = x + 2 * directions[i][0];
        int ny = y + 2 * directions[i][1];

        if (
            nx > 0 && nx < MAZE_SIZE - 1 &&
            ny > 0 && ny < MAZE_SIZE - 1 &&
            maze[ny][nx] == 1
        ) {
            maze[y + directions[i][1]][x + directions[i][0]] = 0;
            dfs_algorithm(maze, nx, ny);
        }
    }
}

void generate_maze(GameState *game_state) {
    for (int i = 0; i < MAZE_SIZE; i++) {
        for (int j = 0; j < MAZE_SIZE; j++) {
            game_state->maze[i][j] = 1;
        }
    }

    dfs_algorithm(game_state->maze, 1, 1);

    game_state->dest_x = MAZE_SIZE - 2;
    game_state->dest_y = MAZE_SIZE - 2;

    int num_upgrades = MAX_PLAYERS - 1;
    place_upgrades(game_state, num_upgrades);
}

void place_upgrades(GameState *game_state, int num_upgrades) {
    int placed = 0;

    while (placed < num_upgrades) {
        int x = rand() % (MAZE_SIZE - 2) + 1;
        int y = rand() % (MAZE_SIZE - 2) + 1;

        if (game_state->maze[y][x] == 0 && !(x == game_state->dest_x && y == game_state->dest_y)) {
            game_state->maze[y][x] = 2;
            placed++;
        }
    }
}

void print_maze(SharedMemory *shared_mem, int player_id, FILE *stream) {
    int radius = shared_mem->players[player_id].radius;
    int px = shared_mem->players[player_id].pos_x;
    int py = shared_mem->players[player_id].pos_y;

    for (int i = 0; i < MAZE_SIZE; i++) {
        for (int j = 0; j < MAZE_SIZE; j++) {
            int is_visible = 0;
            int player_on_pos = -1;
            int player_count = 0;

            if (abs(i - py) <= radius && abs(j - px) <= radius) {
                is_visible = 1;
            }

            if (!is_visible) {
                fprintf(stream, "? ");
                continue;
            }

            for (int p = 0; p < MAX_PLAYERS; p++) {
                if (shared_mem->players[p].socket_id != 0 &&
                    shared_mem->players[p].pos_x == j &&
                    shared_mem->players[p].pos_y == i) {
                    player_count++;
                    player_on_pos = p;
                }
            }

            if (i == shared_mem->game_state.dest_y && j == shared_mem->game_state.dest_x) {
                fprintf(stream, "D ");
            } else if (shared_mem->game_state.maze[i][j] == 2) {
                fprintf(stream, "U ");
            } else if (player_count > 1) {
                fprintf(stream, "X ");
            } else if (player_count == 1) {
                if (player_on_pos == player_id) {
                    fprintf(stream, "P ");
                } else {
                    fprintf(stream, "%d ", player_on_pos);
                }
            } else if (shared_mem->game_state.maze[i][j] == 1) {
                fprintf(stream, "# ");
            } else {
                fprintf(stream, "  ");
            }
        }
        fprintf(stream, "\n");
    }
}


bool can_move_there(GameState game_state, int x, int y) {
    return ((x >= 0 && x < MAZE_SIZE) && (y >= 0 && y < MAZE_SIZE) && (game_state.maze[y][x] == 0 || game_state.maze[y][x] == 2));
}

bool move_player(SharedMemory *shared_mem, int player_id, char direction) {
    if (shared_mem->players[player_id].score > 0) {
        return false;
    }

    int x = shared_mem->players[player_id].pos_x;
    int y = shared_mem->players[player_id].pos_y;

    switch (direction) {
        case 'w': y--; break; // Hore
        case 'a': x--; break; // Dolava
        case 's': y++; break; // Dole
        case 'd': x++; break; // Doprava
        default: return false;
    }

    if (can_move_there(shared_mem->game_state, x, y)) {
        shared_mem->players[player_id].pos_x = x;
        shared_mem->players[player_id].pos_y = y;

        if (shared_mem->game_state.maze[y][x] == 2) {
            if (shared_mem->players[player_id].radius == 1) {
                shared_mem->game_state.maze[y][x] = 0;
                shared_mem->players[player_id].radius = 3;
            }
        }

        if (x == shared_mem->game_state.dest_x && y == shared_mem->game_state.dest_y) {
            if (shared_mem->players[player_id].score == 0) {
                double current = time(NULL);
                shared_mem->players[player_id].time = current - shared_mem->players[player_id].start_time;
            }
            return true;
        }
    }

    return false;
}