/*
 * mcu-max wrapper for micro-Max 4.8
 *
 * (C) 2022 Gissio
 *
 * License: MIT
 */

#ifndef MCUMAX_H
#define MCUMAX_H

#include <stdbool.h>

#define MCUMAX_NAME "mcu-max 1.0 (micro-Max 4.8)"
#define MCUMAX_AUTHOR "Gissio, H.G. Muller"

// Invalid position
#define MCUMAX_INVALID 0x80

typedef unsigned char mcumax_square;
typedef unsigned char mcumax_piece;

struct mcumax_move
{
    mcumax_square from;
    mcumax_square to;
};

typedef void (*mcumax_callback)(void *);

/**
 * Piece types
 */
enum
{
    // Bits 0-2: piece
    MCUMAX_EMPTY,
    MCUMAX_PAWN_UPSTREAM,
    MCUMAX_PAWN_DOWNSTREAM,
    MCUMAX_KNIGHT,
    MCUMAX_KING,
    MCUMAX_BISHOP,
    MCUMAX_ROOK,
    MCUMAX_QUEEN,

    // Bits 3-4: 00-empty, 01-white, 10-black
    MCUMAX_WHITE = 0x8,
    MCUMAX_BLACK = 0x10,
};

/**
 * @brief Resets game state.
 */
void mcumax_reset();

/**
 * @brief Gets piece at specified square.
 *
 * @param square A square coded as 0xRF, R: rank (0-7), F: file (0-7).
 * @return The piece.
 */
mcumax_piece mcumax_get_piece(mcumax_square square);

/**
 * @brief Sets position from a FEN string.
 *
 * @param value The FEN string.
 */
void mcumax_set_fen_position(const char *value);

/**
 * @brief Gets current side.
 */
mcumax_piece mcumax_get_current_side();

/**
 * @brief Sets callback: called periodically during search.
 */
void mcumax_set_callback(mcumax_callback callback, void *userdata);

/**
 * @brief Returns a list of valid moves.
 *
 * @param valid_moves Buffer with list of moves.
 * @param valid_moves_max_num Max number of valid moves buffer can store.
 * @return Number of valid moves found.
 */
int mcumax_get_valid_moves(struct mcumax_move *valid_moves, int valid_moves_max_num);

/**
 * @brief Play move.
 *
 * @param move_from Move from position.
 * @param move_to Move to position.
 *
 * @return Game not finished.
 */
bool mcumax_play_move(struct mcumax_move move);

/**
 * @brief Play best move.
 *
 * @param nodes_limit Max nodes to analyze (1000000: Elo 1955, 10000: Elo ~1500)
 * @param move_from Move from position.
 * @param move_to Move to position.
 *
 * @return Game not finished.
 */
bool mcumax_play_best_move(int nodes_limit, struct mcumax_move *move);

/**
 * @brief Stops best move search.
 */
void mcumax_stop_search();

#endif
