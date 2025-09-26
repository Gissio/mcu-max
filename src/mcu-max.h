/*
 * mcu-max
 * Chess game engine for low-resource MCUs
 *
 * (C) 2022-2024 Gissio
 *
 * License: MIT
 *
 * Based on micro-Max 4.8 by H.G. Muller.
 * Compliant with FIDE laws (except for underpromotion).
 */

#if !defined(MCU_MAX_H)
#define MCU_MAX_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#define MCUMAX_ID "mcu-max 1.0.6"
#define MCUMAX_AUTHOR "Gissio"

#define MCUMAX_SQUARE_INVALID 0x80

#define MCUMAX_BOARD_WHITE 0x8
#define MCUMAX_BOARD_BLACK 0x10

#define MCUMAX_MOVE_INVALID \
    (mcumax_move) { MCUMAX_SQUARE_INVALID, MCUMAX_SQUARE_INVALID }

typedef uint8_t mcumax_square;
typedef uint8_t mcumax_piece;

typedef struct
{
    mcumax_square from;
    mcumax_square to;
} mcumax_move;

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

    // Bits 3: color
    MCUMAX_BLACK = 0x8,
};

/**
 * @brief Resets the engine state.
 */
void mcumax_init(void);

/**
 * @brief Sets position from a FEN string.
 *
 * @param value The FEN string.
 */
void mcumax_set_fen_position(const char *value);

/**
 * @brief Returns the piece at the specified square.
 *
 * @param square A square coded as 0xRF, R: rank (0-7), F: file (0-7).
 * @return The piece.
 */
mcumax_piece mcumax_get_piece(mcumax_square square);

/**
 * @brief Returns the current side.
 */
mcumax_piece mcumax_get_current_side(void);

/**
 * @brief Searches valid moves.
 *
 * @param buffer A buffer for storing valid moves.
 * @param buffer_size The buffer size for storing valid moves.
 *
 * @return The number of valid moves.
 */
uint32_t mcumax_search_valid_moves(mcumax_move *buffer, uint32_t buffer_size);

/**
 * @brief Searches the best move.
 *
 * @param node_max The maximum number of nodes to search.
 * @param depth_max The maximum depth to search.
 *
 * @return The best move (MCUMAX_SQUARE_INVALID, MCUMAX_SQUARE_INVALID if none found).
 */
mcumax_move mcumax_search_best_move(uint32_t node_max, uint32_t depth_max);

/**
 * @brief Plays a move.
 *
 * @param move The move.
 * @return The move was played.
 */
bool mcumax_play_move(mcumax_move move);

/**
 * @brief Sets the user callback, which is called periodically during search.
 */
void mcumax_set_callback(mcumax_callback callback, void *userdata);

/**
 * @brief Stops the current search. To be called from the user callback.
 */
void mcumax_stop_search(void);

/**
 * Checks if the king of the given side is in check.
 */
bool mcumax_is_in_check(uint8_t side);

/**
 * Checks if the king of the given side is in checkmate.
 */
bool mcumax_is_in_checkmate(uint8_t side);

/**
 * Checks if the king of the given side is in stalemate.
 */
bool mcumax_is_stalemate(uint8_t side);

/**
 * Exports the current position in FEN format.
 */
void mcumax_get_fen(char* fen_buffer, size_t buffer_size);

typedef struct mcumax_struct {
    uint8_t board[0x80 + 1];
    uint8_t current_side;
    int32_t score;
    uint8_t en_passant_square;
    int32_t non_pawn_material;
#ifdef MCUMAX_HASHING_ENABLED
    uint32_t hash_key;
    uint32_t hash_key2;
#endif
    uint8_t square_from;
    uint8_t square_to;
    uint32_t node_count;
    uint32_t node_max;
    uint32_t depth_max;
    bool stop_search;
    mcumax_callback user_callback;
    void *user_data;
    mcumax_move *valid_moves_buffer;
    uint32_t valid_moves_buffer_size;
    uint32_t valid_moves_num;
} mcumax_struct;

extern mcumax_struct mcumax;

#ifdef __cplusplus
}
#endif

#endif
