#ifndef GAME
#define GAME

#include <stdbool.h>
#include "ipc.h"
#include "types.h"

void generate_maze(GameState *game_state);
void place_upgrades(GameState *game_state, int num_upgrades);
void print_maze(SharedMemory *shared_mem, int player_id, FILE *stream);
bool move_player(SharedMemory *shared_mem, int player_id, char direction);

#endif
