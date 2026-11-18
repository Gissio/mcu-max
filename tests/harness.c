/*
 * mcu-max test harness
 *
 * Drives the engine through a line-based protocol on stdin/stdout, for the
 * Python test scripts:
 *
 *   init              Resets to the starting position.
 *   fen FEN           Sets a position.
 *   getfen            Prints the position as FEN.
 *   moves             Prints the legal moves (e.g. "e2e4 g1f3 ...").
 *   play MOVE         Plays a move (e.g. "e2e4"); prints 1 if played, 0 if not.
 *   best NODES        Prints the best move (or "none") for a node limit.
 *   side              Prints the side to move (0: white, 8: black).
 *   state             Prints the game state (0-3, see mcumax_game_state).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mcu-max.h"

static void print_square(mcumax_square square)
{
    printf("%c%c",
           'a' + MCUMAX_SQUARE_FILE(square),
           '1' + MCUMAX_SQUARE_RANK(square));
}

static mcumax_square parse_square(const char *s)
{
    return MCUMAX_SQUARE(s[0] - 'a', s[1] - '1');
}

int main(void)
{
    static char line[1024];

    mcumax_init();

    while (fgets(line, sizeof(line), stdin))
    {
        line[strcspn(line, "\r\n")] = '\0';

        if (!strncmp(line, "fen ", 4))
            mcumax_set_fen_position(line + 4);
        else if (!strcmp(line, "init"))
            mcumax_init();
        else if (!strcmp(line, "getfen"))
        {
            char fen_string[MCUMAX_FEN_SIZE];
            mcumax_get_fen_position(fen_string);
            printf("%s\n", fen_string);
        }
        else if (!strcmp(line, "moves"))
        {
            mcumax_move moves[256];
            uint32_t moves_num = mcumax_search_valid_moves(moves, 256);

            for (uint32_t i = 0; (i < moves_num) && (i < 256); i++)
            {
                print_square(moves[i].from);
                print_square(moves[i].to);
                printf(" ");
            }
            printf("\n");
        }
        else if (!strncmp(line, "play ", 5) && (strlen(line) >= 9))
        {
            mcumax_move move = {parse_square(line + 5),
                                parse_square(line + 7)};
            printf("%d\n", mcumax_play_move(move));
        }
        else if (!strncmp(line, "best ", 5))
        {
            mcumax_move move = mcumax_search_best_move(atol(line + 5), 30);

            if (move.from == MCUMAX_SQUARE_INVALID)
                printf("none");
            else
            {
                print_square(move.from);
                print_square(move.to);
            }
            printf("\n");
        }
        else if (!strcmp(line, "side"))
            printf("%d\n", mcumax_get_current_side());
        else if (!strcmp(line, "state"))
            printf("%d\n", mcumax_get_game_state());

        fflush(stdout);
    }

    return 0;
}
