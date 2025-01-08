#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "game.h"

int main(int argc, char *argv[]) {
  struct GameState state;
  char direction;
  srand(time(NULL));

  generate_maze(&state);
  printf("Hra Bludisko\n");
  printf("Pohyb hore(w), dole (s), dolava (a), doprava(d).\n");
  printf("Dostante sa do ciela!\n");

  system("stty -icanon -echo");
  while (1) {
    print_maze(&state);

    if (state.pos_x == state.dest_x && state.pos_y == state.dest_y) {
      printf("Dosiahli ste ciel!\n");
      break;
    }

    printf("Zadajte pohyb: ");
    direction = getchar();
    //scanf(" %c", &direction);

    if (direction == 'q' || direction == 'Q') {
      printf("\nHra bola ukoncena.\n");
      break;
    }

    move_player(&state, direction);
  }
  system("stty sane");

  return 0;
}
