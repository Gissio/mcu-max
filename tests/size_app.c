/*
 * Minimal application using every non-FEN function, for measuring the
 * linked code size (see size.py).
 */

#include "mcu-max.h"

static void callback(void *userdata)
{
    if (userdata)
        mcumax_stop_search();
}

int main(void)
{
    mcumax_move moves[64];

    mcumax_init();
    mcumax_set_callback(callback, 0);

    uint32_t moves_num = mcumax_search_valid_moves(moves, 64);
    mcumax_move move = mcumax_search_best_move(1000, 3);
    mcumax_play_move(move);

    return moves_num +
           mcumax_get_piece(0) +
           mcumax_get_current_side() +
           mcumax_get_game_state();
}
