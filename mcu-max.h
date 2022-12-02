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
    MCUMAX_KING,
    MCUMAX_KNIGHT,
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
 * @param valid_moves_buffer List of lists of (move-from, move-to) pairs, terminated by MCUMAX_INVALID.
 * @param valid_moves_buffer_size Size of valid moves buffer.
 */
void mcumax_get_valid_moves(mcumax_square *valid_moves_buffer, int valid_moves_buffer_size);

/**
 * @brief Play move.
 *
 * @param move_from Move from position.
 * @param move_to Move to position.
 *
 * @return Game not finished.
 */
bool mcumax_play_move(mcumax_square move_from, mcumax_square move_to);

/**
 * @brief Play best move.
 *
 * @param nodes_limit Max nodes to analyze (1000000: Elo 1955, 10000: Elo ~1500)
 * @param move_from Move from position.
 * @param move_to Move to position.
 *
 * @return Game not finished.
 */
bool mcumax_play_best_move(int nodes_limit,
                           mcumax_square *move_from, mcumax_square *move_to);

/**
 * @brief Stops best move search.
 */
void mcumax_stop_search();

#endif
