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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mcu-max.h"

// Configuration
// #define MCUMAX_HASHING_ENABLED

// Constants
#define MCUMAX_BOARD_MASK 0x88
#define MCUMAX_BOARD_WHITE 0x8
#define MCUMAX_BOARD_BLACK 0x10
#define MCUMAX_PIECE_MOVED 0x20
#define MCUMAX_SCORE_MAX 8000
#define MCUMAX_DEPTH_MAX 99

enum mcumax_mode
{
    MCUMAX_INTERNAL_NODE,
    MCUMAX_SEARCH_VALID_MOVES,
    MCUMAX_SEARCH_BEST_MOVE,
    MCUMAX_PLAY_MOVE,
};

mcumax_struct mcumax;

static const int8_t mcumax_capture_values[] = {
    0, 2, 2, 7, -1, 8, 12, 23};

static const int8_t mcumax_step_vectors_indices[] = {
    0, 7, -1, 11, 6, 8, 3, 6};

static const int8_t mcumax_step_vectors[] = {
    // Upstream pawn
    -16, -15, -17, 0,
    // Rook
    1, 16, 0,
    // King, queen
    1, 16, 15, 17, 0,
    // Knight
    14, 18, 31, 33, 0};

static const int8_t mcumax_board_setup[] = {
    MCUMAX_ROOK,
    MCUMAX_KNIGHT,
    MCUMAX_BISHOP,
    MCUMAX_QUEEN,
    MCUMAX_KING,
    MCUMAX_BISHOP,
    MCUMAX_KNIGHT,
    MCUMAX_ROOK,
};

#ifdef MCUMAX_HASHING_ENABLED

#define MCUMAX_HASH_SCRAMBLE_TABLE_SIZE 1035
#define MCUMAX_HASH_TABLE_SIZE (1 << 24)

#define HashScramble(A, B)                \
    *(uint32_t *)(mcumax_scramble_table + \
                  A + (B & 8) + MCUMAX_SQUARE_INVALID * (B & 0b111))
#define Hash(A)                                                      \
    HashScramble(step_square_to + A, mcumax.board[step_square_to]) - \
        HashScramble(scan_square_from + A, scan_piece) -             \
        HashScramble(capture_square + A, capture_piece)

static uint8_t mcumax_scramble_table[MCUMAX_HASH_SCRAMBLE_TABLE_SIZE]; /* hash translation table */

struct HashEntry
{
    uint32_t key2;
    int32_t score;
    uint8_t square_from;
    uint8_t square_to;
    uint8_t depth;
};

static struct HashEntry mcumax_hash_table[MCUMAX_HASH_TABLE_SIZE];

#endif

typedef bool (*mcumax_move_callback)(mcumax_move move);

static int32_t mcumax_search(int32_t alpha,
                             int32_t beta,
                             int32_t score,
                             uint8_t en_passant_square,
                             uint8_t depth,
                             enum mcumax_mode mode);

// Recursive minimax search
// (alpha,beta)=window, score=current evaluation score, en_passant_square=e.p. sqr.
// depth=depth, in_root=in_root; returns score
static int32_t mcumax_search(int32_t alpha,
                             int32_t beta,
                             int32_t score,
                             uint8_t en_passant_square,
                             uint8_t depth,
                             enum mcumax_mode mode)
{
    if (mcumax.user_callback)
        mcumax.user_callback(mcumax.user_data);

    uint8_t iter_depth;
    int32_t iter_score;
    uint8_t iter_square_from;
    uint8_t iter_square_to;

#ifdef MCUMAX_HASHING_ENABLED
    int32_t hash_key;
    int32_t hash_key2;
#endif

    uint8_t square_start;

    uint8_t square_from;
    uint8_t square_to;

    uint8_t replay_move;
    int32_t null_move_score;

    uint8_t scan_piece;
    uint8_t scan_piece_type;

    int8_t step_vector;
    int8_t step_vector_index;

    uint8_t castling_skip_square;
    uint8_t castling_rook_square;

    uint8_t capture_square;
    uint8_t capture_piece;
    int32_t capture_piece_value;

    uint8_t step_depth;
    int32_t step_alpha;
    int32_t step_score;
    int32_t step_score_new;

    // Adj. window: delay bonus
    alpha -= alpha < score;
    beta -= beta <= score;

#ifdef MCUMAX_HASHING_ENABLED
    // Lookup pos. in hash table
    struct HashEntry *hash_entry = mcumax_hash_table +
                                   ((mcumax.hash_key +
                                     mcumax.current_side * en_passant_square) &
                                    MCUMAX_HASH_TABLE_SIZE - 1);

    iter_depth = hash_entry->depth;
    iter_score = hash_entry->score;
    iter_square_from = hash_entry->square_from;
    iter_square_to = hash_entry->square_to;

    // Resume at stored depth
    if ((hash_entry->key2 != mcumax.hash_key2) ||
        (mode != MCUMAX_INTERNAL_NODE) || // Miss: other pos. or empty
        !(((iter_score <= alpha) ||
           (iter_square_from & 0x8)) &&
          ((iter_score >= beta) ||
           (iter_square_from & MCUMAX_SQUARE_INVALID)))) // Or window incompatible
    {
        // Start iteration from scratch
        iter_depth =
            iter_square_to = 0;
    }

    // Start at best-move hint
    iter_square_from &= ~MCUMAX_BOARD_MASK;

    hash_key = mcumax.hash_key;
    hash_key2 = mcumax.hash_key2;
#else
    iter_depth =
        iter_score =
            iter_square_from =
                iter_square_to = 0;
#endif

    // Min depth = 2 iterative deepening loop
    // root: deepen upto time
    // time's up: go do best
    while ((iter_depth++ < depth) ||
           (iter_depth < 3) ||
           ((mode != MCUMAX_INTERNAL_NODE) &&
            (mcumax.square_from == MCUMAX_SQUARE_INVALID) &&
            (((mcumax.node_count < mcumax.node_max) &&
              (iter_depth <= mcumax.depth_max)) ||
             (mcumax.square_from = iter_square_from,
              mcumax.square_to = iter_square_to & ~MCUMAX_BOARD_MASK,
              iter_depth = 3))))
    {
        if (mcumax.stop_search)
            break;

        // Start scan at previous best
        square_from =
            square_start = (mode != MCUMAX_SEARCH_VALID_MOVES)
                               ? iter_square_from
                               : 0;

        // Request try noncastling first
        replay_move = iter_square_to & MCUMAX_SQUARE_INVALID;

        // Change side
        mcumax.current_side ^= 0x18;

        // Search null move
        null_move_score = (iter_depth > 2) && (beta != -MCUMAX_SCORE_MAX)
                              ? mcumax_search(-beta,
                                              1 - beta,
                                              -score,
                                              MCUMAX_SQUARE_INVALID,
                                              iter_depth - 3,
                                              MCUMAX_INTERNAL_NODE)
                              : MCUMAX_SCORE_MAX;

        // Change side
        mcumax.current_side ^= 0x18;

        // Prune if > beta unconsidered:static eval
        iter_score = (-null_move_score < beta) ||
                             (mcumax.non_pawn_material > 35)
                         ? (iter_depth - 2)
                               ? -MCUMAX_SCORE_MAX
                               : score
                         : -null_move_score;

        // Node count (for timing)
        mcumax.node_count++;

        do
        {
            // Scan board looking for
            scan_piece = mcumax.board[square_from];

            // Own piece (inefficient!)
            if (scan_piece & mcumax.current_side)
            {
                // p = piece type (set r>0)
                step_vector = scan_piece_type = (scan_piece & 0b111);

                // First step vector for piece
                step_vector_index = mcumax_step_vectors_indices[scan_piece_type];

                // Loop over directions o[]
                while ((step_vector = ((scan_piece_type > 2) &&
                                       (step_vector < 0))
                                          ? -step_vector
                                          : -mcumax_step_vectors[++step_vector_index]))
                {
                replay:
                    // Resume normal after best
                    square_to = square_from;

                    castling_skip_square =
                        castling_rook_square = MCUMAX_SQUARE_INVALID;

                    // y traverses ray, or:
                    do
                    {
                        // Sneak in previous best move
                        capture_square =
                            square_to =
                                replay_move
                                    ? (iter_square_to ^ replay_move)
                                    : (square_to + step_vector);

                        // Board edge hit
                        if (square_to & MCUMAX_BOARD_MASK)
                            break;

                        // Bad castling
                        if ((en_passant_square != MCUMAX_SQUARE_INVALID) &&
                            mcumax.board[en_passant_square] &&
                            ((square_to - en_passant_square) < 2) &&
                            ((en_passant_square - square_to) < 2))
                            iter_score = MCUMAX_SCORE_MAX;

                        // Shift capture square if en-passant
                        if ((scan_piece_type < 3) &&
                            (square_to == en_passant_square))
                            capture_square ^= 16;

                        capture_piece = mcumax.board[capture_square];

                        // Capture own, bad pawn mode
                        if ((capture_piece & mcumax.current_side) ||
                            ((scan_piece_type < 3) &&
                             !((square_to - square_from) & 0b111) - !capture_piece))
                            break;

                        // Value of captured piece
                        capture_piece_value = 37 * mcumax_capture_values[capture_piece & 0b111] +
                                              (capture_piece & 0xc0);

                        // King capture
                        if (capture_piece_value < 0)
                        {
                            iter_score = MCUMAX_SCORE_MAX;
                            iter_depth = MCUMAX_DEPTH_MAX - 1;
                        }

                        // Abort on fail high
                        if ((iter_score >= beta) &&
                            (iter_depth > 1))
                            goto cutoff;

                        // MVV/LVA scoring if depth == 1
                        step_score = (iter_depth != 1)
                                         ? score
                                         : capture_piece_value - scan_piece_type;

                        // All captures if depth == 2
                        if ((iter_depth - !capture_piece) > 1)
                        {
                            // Center positional score
                            step_score = (scan_piece_type < 6)
                                             ? mcumax.board[square_from + 0x8] -
                                                   mcumax.board[square_to + 0x8]
                                             : 0;

                            mcumax.board[castling_rook_square] =
                                mcumax.board[capture_square] =
                                    mcumax.board[square_from] = 0;

                            // Do move, set non-virgin
                            mcumax.board[square_to] = scan_piece | MCUMAX_PIECE_MOVED;

                            // Castling: put rook & score
                            if (!(castling_rook_square & MCUMAX_BOARD_MASK))
                            {
                                mcumax.board[castling_skip_square] = mcumax.current_side + 6;
                                step_score += 50;
                            }

                            // Freeze king in mid-game
                            step_score -= ((scan_piece_type != 4) ||
                                           (mcumax.non_pawn_material > 30))
                                              ? 0
                                              : 20;

                            // Pawns
                            if (scan_piece_type < 3)
                            {
                                step_score -=
                                    9 * ((((square_from - 2) & MCUMAX_BOARD_MASK) ||
                                          mcumax.board[square_from - 2] - scan_piece) +
                                         // Structure, undefended
                                         (((square_from + 2) & MCUMAX_BOARD_MASK) ||
                                          mcumax.board[square_from + 2] - scan_piece) -
                                         1 +
                                         // Squares plus bias
                                         (mcumax.board[square_from ^ 0x10] ==
                                          (mcumax.current_side + 36))) // Cling to magnetic king
                                    - (mcumax.non_pawn_material >> 2); // End-game Pawn-push bonus

                                // Promotion / passer bonus
                                capture_piece_value +=
                                    step_alpha =
                                        (square_to + step_vector + 1) & MCUMAX_SQUARE_INVALID
                                            ? (647 - scan_piece_type)
                                            : 2 * (scan_piece & (square_to + 0x10) & 0x20);

                                // Upgrade pawn or convert to queen
                                mcumax.board[square_to] += step_alpha;
                            }

#ifdef MCUMAX_HASHING_ENABLED
                            mcumax.hash_key += Hash(0);
                            mcumax.hash_key2 += Hash(8) + castling_rook_square - MCUMAX_SQUARE_INVALID;
#endif

                            // New score & alpha
                            step_score += score + capture_piece_value;
                            step_alpha = iter_score > alpha
                                             ? iter_score
                                             : alpha;

                            // New depth, reduce non-capture
                            step_depth = iter_depth - 1 -
                                         ((iter_depth > 5) &&
                                          (scan_piece_type > 2) &&
                                          !capture_piece &&
                                          !replay_move);

                            // Extend 1 ply if in check
                            if (!((mcumax.non_pawn_material > 30) ||
                                  (null_move_score - MCUMAX_SCORE_MAX) ||
                                  (iter_depth < 3) ||
                                  (capture_piece &&
                                   (scan_piece_type != 4))))
                                step_depth = iter_depth;

                            // Futility, recursive evaluation of reply
                            do
                            {
                                // Change side
                                mcumax.current_side ^= 0x18;

                                step_score_new = ((mode == MCUMAX_SEARCH_VALID_MOVES) ||
                                                  (step_depth > 2) ||
                                                  (step_score > step_alpha))
                                                     ? -mcumax_search(-beta,
                                                                      -step_alpha,
                                                                      -step_score,
                                                                      castling_skip_square,
                                                                      step_depth,
                                                                      MCUMAX_INTERNAL_NODE)
                                                     : step_score;

                                // Change side
                                mcumax.current_side ^= 0x18;
                            } while ((step_score_new > alpha) &&
                                     (++step_depth < iter_depth));

                            // No fail: re-search unreduced
                            step_score = step_score_new;

                            if ((mode == MCUMAX_PLAY_MOVE) &&
                                (step_score != -MCUMAX_SCORE_MAX) &&
                                (square_from == mcumax.square_from) &&
                                (square_to == mcumax.square_to))
                            {
                                // Playing move
                                mcumax.score = -score - capture_piece_value;
                                mcumax.en_passant_square = castling_skip_square;

#ifdef MCUMAX_HASHING_ENABLED
                                // Lock game in hash as draw
                                hash_entry->depth = MCUMAX_DEPTH_MAX;
                                hash_entry->score = 0;
#endif

                                // Total captured material
                                mcumax.non_pawn_material += capture_piece_value >> 7;

                                // Change side
                                mcumax.current_side ^= 0x18;

                                // Captured non-pawn material
                                return beta;
                            }

#ifdef MCUMAX_HASHING_ENABLED
                            mcumax.hash_key = hash_key;
                            mcumax.hash_key2 = hash_key2;
#endif

                            // Undo move
                            mcumax.board[castling_rook_square] = mcumax.current_side + 6;
                            mcumax.board[castling_skip_square] = mcumax.board[square_to] = 0;
                            mcumax.board[square_from] = scan_piece;
                            mcumax.board[capture_square] = capture_piece;

                            if ((mode == MCUMAX_SEARCH_BEST_MOVE) &&
                                (step_score != -MCUMAX_SCORE_MAX) &&
                                (square_from == mcumax.square_from) &&
                                (square_to == mcumax.square_to))
                                // Searching best move
                                return beta;

                            if ((mode == MCUMAX_SEARCH_VALID_MOVES) &&
                                (step_score != -MCUMAX_SCORE_MAX) &&
                                (mcumax.square_from == MCUMAX_SQUARE_INVALID) &&
                                (iter_depth == 3) &&
                                !replay_move)
                            {
                                // Searching valid moves
                                mcumax_move move = {square_from, square_to};

                                if (mcumax.valid_moves_num < mcumax.valid_moves_buffer_size)
                                    mcumax.valid_moves_buffer[mcumax.valid_moves_num] = move;

                                mcumax.valid_moves_num++;
                            }
                        }

                        // New best, update max,best
                        if (step_score > iter_score)
                        {
                            // Mark non-double
                            iter_score = step_score;
                            iter_square_from = square_from;
                            iter_square_to = square_to |
                                             (castling_skip_square & MCUMAX_SQUARE_INVALID);
                        }

                        if (replay_move)
                        {
                            // Redo after doing old best
                            replay_move = 0;

                            goto replay;
                        }

                        // Not first step, moved before
                        if ((square_from + step_vector - square_to) ||
                            (scan_piece & MCUMAX_PIECE_MOVED) ||
                            // No pawn and no lateral king move
                            ((scan_piece_type > 2) &&
                             (((scan_piece_type != 4) ||
                               (step_vector_index != 7) ||
                               // No virgin rook in corner
                               (mcumax.board[castling_rook_square =
                                                 ((square_from + 3) ^
                                                  ((step_vector >> 1) & 0b111))] -
                                mcumax.current_side - 6) ||
                               // No two empty squares next to rook
                               mcumax.board[castling_rook_square ^ 1] ||
                               mcumax.board[castling_rook_square ^ 2]))))
                            // Fake capture for nonsliding
                            capture_piece += (scan_piece_type < 5);
                        else
                            // Enable en-passant
                            castling_skip_square = square_to;

                        // If no capture, continue ray
                    } while (!capture_piece);
                }
            }

            // Next square of board, wrap
        } while ((square_from = ((square_from + 9) &
                                 ~MCUMAX_BOARD_MASK)) != square_start);

    cutoff:
        // Check test thru NM best loses king: (stale)mate
        if ((iter_score == -MCUMAX_SCORE_MAX) &&
            (null_move_score != MCUMAX_SCORE_MAX))
            iter_score = 0;

#ifdef MCUMAX_HASHING_ENABLED
        // Protect game history
        if (hash_entry->depth < MCUMAX_DEPTH_MAX)
        {
            hash_entry->key2 = mcumax.hash_key2;
            hash_entry->score = iter_score;
            hash_entry->depth = iter_depth;

            // Move, type (bound/exact)
            hash_entry->square_from = iter_square_from |
                                      8 * (iter_score > alpha) |
                                      MCUMAX_SQUARE_INVALID * (iter_score < beta);
            hash_entry->square_to = iter_square_to;
        }
#endif

        // Kibitz
        // if (in_root)
        //     printf("%2d %6d %10d %c%c%c%c\n",
        //         iter_depth - 2,
        //         iter_score,
        //         mcumax.node_count,
        //         'a' + (iter_square_from & 0b111),
        //         '8' - (iter_square_from >> 4),
        //         'a' + (iter_square_to & 0b111),
        //         '8' - (iter_square_to >> 4 & 0b111));
    }

    // Delayed-loss bonus
    return iter_score += iter_score < score;
}

/***************************************************************************/

void mcumax_init()
{
    for (uint32_t x = 0; x < 8; x++)
    {
        // Setup pieces (left side)
        mcumax.board[0x10 * 0 + x] = MCUMAX_BOARD_BLACK | mcumax_board_setup[x];
        mcumax.board[0x10 * 1 + x] = MCUMAX_BOARD_BLACK | MCUMAX_PAWN_DOWNSTREAM;
        for (uint32_t y = 2; y < 6; y++)
            mcumax.board[0x10 * y + x] = MCUMAX_EMPTY;
        mcumax.board[0x10 * 6 + x] = MCUMAX_BOARD_WHITE | MCUMAX_PAWN_UPSTREAM;
        mcumax.board[0x10 * 7 + x] = MCUMAX_BOARD_WHITE | mcumax_board_setup[x];

        // Setup weights (right side)
        for (uint32_t y = 0; y < 8; y++)
            mcumax.board[16 * y + x + 8] = (x - 4) * (x - 4) + (y - 4) * (y - 3);
    }
    mcumax.current_side = MCUMAX_BOARD_WHITE;

    mcumax.score = 0;
    mcumax.en_passant_square = MCUMAX_SQUARE_INVALID;
    mcumax.non_pawn_material = 0;

#ifdef MCUMAX_HASHING_ENABLED
    mcumax.hash_key = 0;
    mcumax.hash_key2 = 0;

    memset(mcumax_hash_table, 0, sizeof(mcumax_hash_table));

    // for (uint32_t i = MCUMAX_HASH_SCRAMBLE_TABLE_SIZE - 1; i > MCUMAX_BOARD_MASK; i--)
    //     mcumax_scramble_table[i] = rand() & 0xff;

    srand(1);
    for (uint32_t i = 0; i < 1035; i++)
        mcumax_scramble_table[i] =
            ((rand() & 0xff) << 0) |
            ((rand() & 0xff) << 8) |
            ((rand() & 0xff) << 16) |
            ((rand() & 0xff) << 24);
#endif
}

static mcumax_square mcumax_set_piece(mcumax_square square, mcumax_piece piece)
{
    if (square & MCUMAX_BOARD_MASK)
        return square;

    mcumax.board[square] = piece ? (piece | MCUMAX_PIECE_MOVED) : piece;

    return square + 1;
}

mcumax_piece mcumax_get_piece(mcumax_square square)
{
    if (square & MCUMAX_BOARD_MASK)
        return MCUMAX_EMPTY;

    return (mcumax.board[square] & 0xf) ^ MCUMAX_BLACK;
}

void mcumax_set_fen_position(const char *fen_string)
{
    mcumax_init();

    uint32_t field_index = 0;
    uint32_t board_index = 0;

    char c;
    while ((c = *fen_string++))
    {
        if (c == ' ')
        {
            if (field_index < 4)
                field_index++;

            continue;
        }

        switch (field_index)
        {
        case 0:
            if (board_index < 0x80)
            {
                switch (c)
                {
                case '8':
                case '7':
                case '6':
                case '5':
                case '4':
                case '3':
                case '2':
                case '1':
                    for (int32_t i = 0; i < (c - '0'); i++)
                        board_index = mcumax_set_piece(board_index, MCUMAX_EMPTY);

                    break;

                case 'P':
                    board_index = mcumax_set_piece(board_index, MCUMAX_PAWN_UPSTREAM | MCUMAX_BOARD_WHITE);

                    break;

                case 'N':
                    board_index = mcumax_set_piece(board_index, MCUMAX_KNIGHT | MCUMAX_BOARD_WHITE);

                    break;

                case 'B':
                    board_index = mcumax_set_piece(board_index, MCUMAX_BISHOP | MCUMAX_BOARD_WHITE);

                    break;

                case 'R':
                    board_index = mcumax_set_piece(board_index, MCUMAX_ROOK | MCUMAX_BOARD_WHITE);

                    break;

                case 'Q':
                    board_index = mcumax_set_piece(board_index, MCUMAX_QUEEN | MCUMAX_BOARD_WHITE);

                    break;

                case 'K':
                    board_index = mcumax_set_piece(board_index, MCUMAX_KING | MCUMAX_BOARD_WHITE);

                    break;

                case 'p':
                    board_index = mcumax_set_piece(board_index, MCUMAX_PAWN_DOWNSTREAM | MCUMAX_BOARD_BLACK);

                    break;

                case 'n':
                    board_index = mcumax_set_piece(board_index, MCUMAX_KNIGHT | MCUMAX_BOARD_BLACK);

                    break;

                case 'b':
                    board_index = mcumax_set_piece(board_index, MCUMAX_BISHOP | MCUMAX_BOARD_BLACK);

                    break;

                case 'r':
                    board_index = mcumax_set_piece(board_index, MCUMAX_ROOK | MCUMAX_BOARD_BLACK);

                    break;

                case 'q':
                    board_index = mcumax_set_piece(board_index, MCUMAX_QUEEN | MCUMAX_BOARD_BLACK);

                    break;

                case 'k':
                    board_index = mcumax_set_piece(board_index, MCUMAX_KING | MCUMAX_BOARD_BLACK);

                    break;

                case '/':
                    board_index = (board_index < 0x80) ? (board_index & 0xf0) + 0x10 : board_index;

                    break;
                }
            }
            break;

        case 1:
            switch (c)
            {
            case 'w':
                mcumax.current_side = MCUMAX_BOARD_WHITE;

                break;

            case 'b':
                mcumax.current_side = MCUMAX_BOARD_BLACK;

                break;
            }
            break;

        case 2:
            switch (c)
            {
            case 'K':
                mcumax.board[0x74] &= ~MCUMAX_PIECE_MOVED;
                mcumax.board[0x77] &= ~MCUMAX_PIECE_MOVED;

                break;

            case 'Q':
                mcumax.board[0x74] &= ~MCUMAX_PIECE_MOVED;
                mcumax.board[0x70] &= ~MCUMAX_PIECE_MOVED;

                break;

            case 'k':
                mcumax.board[0x04] &= ~MCUMAX_PIECE_MOVED;
                mcumax.board[0x07] &= ~MCUMAX_PIECE_MOVED;

                break;

            case 'q':
                mcumax.board[0x04] &= ~MCUMAX_PIECE_MOVED;
                mcumax.board[0x00] &= ~MCUMAX_PIECE_MOVED;

                break;
            }

            break;

        case 3:
            switch (c)
            {
            case 'a':
            case 'b':
            case 'c':
            case 'd':
            case 'e':
            case 'f':
            case 'g':
            case 'h':
                mcumax.en_passant_square &= 0x7f;
                mcumax.en_passant_square |= (c - 'a');

                break;

            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
                mcumax.en_passant_square &= 0x7f;
                mcumax.en_passant_square |= 16 * ('8' - c);

                break;
            }

            break;
        }
    }
}

mcumax_piece mcumax_get_current_side(void)
{
    return mcumax.current_side;
}

static int32_t mcumax_start_search(enum mcumax_mode mode,
                                   mcumax_move move,
                                   uint32_t depth_max,
                                   uint32_t node_max)
{
    mcumax.square_from = move.from;
    mcumax.square_to = move.to;

    mcumax.node_max = node_max;
    mcumax.node_count = 0;
    mcumax.depth_max = depth_max;

    mcumax.stop_search = false;

    return mcumax_search(-MCUMAX_SCORE_MAX,
                         MCUMAX_SCORE_MAX,
                         mcumax.score,
                         mcumax.en_passant_square,
                         3,
                         mode);
}

uint32_t mcumax_search_valid_moves(mcumax_move *valid_moves_buffer, uint32_t valid_moves_buffer_size)
{
    mcumax.valid_moves_num = 0;
    mcumax.valid_moves_buffer = valid_moves_buffer;
    mcumax.valid_moves_buffer_size = valid_moves_buffer_size;

    mcumax_start_search(MCUMAX_SEARCH_VALID_MOVES, MCUMAX_MOVE_INVALID, 0, 0);

    return mcumax.valid_moves_num;
}

mcumax_move mcumax_search_best_move(uint32_t node_max, uint32_t depth_max)
{
    int32_t score = mcumax_start_search(MCUMAX_SEARCH_BEST_MOVE,
                                        MCUMAX_MOVE_INVALID, depth_max + 3, node_max);

    if (score == MCUMAX_SCORE_MAX)
        return (mcumax_move){mcumax.square_from, mcumax.square_to};
    else
        return MCUMAX_MOVE_INVALID;
}

bool mcumax_play_move(mcumax_move move)
{
    return mcumax_start_search(MCUMAX_PLAY_MOVE, move, 0, 0) == MCUMAX_SCORE_MAX;
}

void mcumax_set_callback(mcumax_callback callback, void *userdata)
{
    mcumax.user_callback = callback;
    mcumax.user_data = userdata;
}

void mcumax_stop_search(void)
{
    mcumax.stop_search = true;
}

bool mcumax_is_in_check(uint8_t side) {
    uint8_t king_mask = (side == MCUMAX_BOARD_WHITE) ? MCUMAX_BOARD_WHITE : MCUMAX_BOARD_BLACK;
    uint8_t enemy_mask = (side == MCUMAX_BOARD_WHITE) ? MCUMAX_BOARD_BLACK : MCUMAX_BOARD_WHITE;
    mcumax_square king_square = MCUMAX_SQUARE_INVALID;
    
    // Find the king
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            mcumax_square sq = y * 16 + x;
            uint8_t raw = mcumax.board[sq];
            if ((raw & king_mask) && ((raw & 0b111) == MCUMAX_KING)) {
                king_square = sq;
                break;
            }
        }
        if (king_square != MCUMAX_SQUARE_INVALID) break;
    }
    
    if (king_square == MCUMAX_SQUARE_INVALID)
        return false;
    
    // Directions: horizontal/vertical then diagonals
    int directions[8] = {1, -1, 16, -16, 15, -15, 17, -17};
    
    // Scan rays for rooks, bishops and queens
    for (int d = 0; d < 8; d++) {
        int current_sq = king_square;
        
        while (1) {
            current_sq += directions[d];
            
            // Check 0x88 board limits
            if (current_sq & 0x88) {
                break;
            }
            
            uint8_t raw = mcumax.board[current_sq];
            if (raw) {
                // If it's an enemy piece
                if (raw & enemy_mask) {
                    int type = raw & 0b111;
                    // Rook or queen on lines/columns (directions 0-3)
                    if ((d < 4) && (type == MCUMAX_ROOK || type == MCUMAX_QUEEN)) {
                        return true;
                    }
                    // Bishop or queen on diagonals (directions 4-7)
                    if ((d >= 4) && (type == MCUMAX_BISHOP || type == MCUMAX_QUEEN)) {
                        return true;
                    }
                }
                // A piece blocks the ray, stop
                break;
            }
        }
    }
    
    // Check knights
    int knight_moves[8] = {14, 18, 31, 33, -14, -18, -31, -33};
    for (int i = 0; i < 8; i++) {
        int sq = king_square + knight_moves[i];
        if (!(sq & 0x88)) {
            uint8_t raw = mcumax.board[sq];
            if ((raw & enemy_mask) && ((raw & 0b111) == MCUMAX_KNIGHT)) {
                return true;
            }
        }
    }
    
    // Check pawns
    int pawn_dir = (side == MCUMAX_BOARD_WHITE) ? -16 : 16;
    int pawn_attacks[2] = {pawn_dir - 1, pawn_dir + 1};
    for (int i = 0; i < 2; i++) {
        int sq = king_square + pawn_attacks[i];
        if (!(sq & 0x88)) {
            uint8_t raw = mcumax.board[sq];
            if (raw & enemy_mask) {
                int type = raw & 0b111;
                // White pawn attacks upward (UPSTREAM), black pawn downward (DOWNSTREAM)
                if ((side == MCUMAX_BOARD_WHITE && type == MCUMAX_PAWN_DOWNSTREAM) ||
                    (side == MCUMAX_BOARD_BLACK && type == MCUMAX_PAWN_UPSTREAM)) {
                    return true;
                }
            }
        }
    }
    
    // Check opposing king (adjacent attack)
    int king_moves[8] = {1, -1, 16, -16, 15, -15, 17, -17};
    for (int i = 0; i < 8; i++) {
        int sq = king_square + king_moves[i];
        if (!(sq & 0x88)) {
            uint8_t raw = mcumax.board[sq];
            if ((raw & enemy_mask) && ((raw & 0b111) == MCUMAX_KING)) {
                return true;
            }
        }
    }
    
    return false;
}

bool mcumax_is_in_checkmate(uint8_t side) {
    // 1. Check if the king is in check (necessary condition for mate)
    if (!mcumax_is_in_check(side)) {
        return false; // Not in check = not mate
    }
    
    // 2. Save the current engine state
    uint8_t board_backup[sizeof(mcumax.board)];
    memcpy(board_backup, mcumax.board, sizeof(mcumax.board));
    uint8_t current_side_backup = mcumax.current_side;
    uint8_t en_passant_backup = mcumax.en_passant_square;
    int32_t score_backup = mcumax.score;
    int32_t npm_backup = mcumax.non_pawn_material;
    
    // 3. Make sure it's the tested side's turn
    mcumax.current_side = side;
    
    // 4. Generate all possible legal moves
    mcumax_move valid_moves[256]; // Buffer large enough for all possible moves
    uint32_t moves_count = mcumax_search_valid_moves(valid_moves, 256);
    
    // 5. Test each move to see if it gets the king out of check
    for (uint32_t i = 0; i < moves_count; i++) {
        // Save state before playing the move
        uint8_t temp_board[sizeof(mcumax.board)];
        memcpy(temp_board, mcumax.board, sizeof(mcumax.board));
        uint8_t temp_en_passant = mcumax.en_passant_square;
        int32_t temp_score = mcumax.score;
        int32_t temp_npm = mcumax.non_pawn_material;
        
        // Play the move temporarily
        if (mcumax_play_move(valid_moves[i])) {
            // Check if the king is still in check after this move
            bool still_in_check = mcumax_is_in_check(side);
            
            // Restore state
            memcpy(mcumax.board, temp_board, sizeof(mcumax.board));
            mcumax.en_passant_square = temp_en_passant;
            mcumax.score = temp_score;
            mcumax.non_pawn_material = temp_npm;
            mcumax.current_side = side;
            
            // If this move gets the king out of check, it's not mate
            if (!still_in_check) {
                // Restore full engine state
                memcpy(mcumax.board, board_backup, sizeof(mcumax.board));
                mcumax.current_side = current_side_backup;
                mcumax.en_passant_square = en_passant_backup;
                mcumax.score = score_backup;
                mcumax.non_pawn_material = npm_backup;
                return false;
            }
        } else {
            // The move could not be played, restore anyway
            memcpy(mcumax.board, temp_board, sizeof(mcumax.board));
            mcumax.en_passant_square = temp_en_passant;
            mcumax.score = temp_score;
            mcumax.non_pawn_material = temp_npm;
            mcumax.current_side = side;
        }
    }
    
    // 6. Restore full engine state
    memcpy(mcumax.board, board_backup, sizeof(mcumax.board));
    mcumax.current_side = current_side_backup;
    mcumax.en_passant_square = en_passant_backup;
    mcumax.score = score_backup;
    mcumax.non_pawn_material = npm_backup;
    
    // 7. If no move gets out of check, it's mate
    return true;
}

bool mcumax_is_stalemate(uint8_t side) {
    // 1. Check if the king is NOT in check (necessary condition for stalemate)
    if (mcumax_is_in_check(side)) {
        return false; // In check = not stalemate (potentially mate)
    }
    
    // 2. Save the current engine state
    uint8_t board_backup[sizeof(mcumax.board)];
    memcpy(board_backup, mcumax.board, sizeof(mcumax.board));
    uint8_t current_side_backup = mcumax.current_side;
    uint8_t en_passant_backup = mcumax.en_passant_square;
    int32_t score_backup = mcumax.score;
    int32_t npm_backup = mcumax.non_pawn_material;
    
    // 3. Make sure it's the tested side's turn
    mcumax.current_side = side;
    
    // 4. Generate all possible legal moves
    mcumax_move valid_moves[256]; // Buffer large enough for all possible moves
    uint32_t moves_count = mcumax_search_valid_moves(valid_moves, 256);
    
    // 5. Test each move to see if it is really legal (does not put own king in check)
    for (uint32_t i = 0; i < moves_count; i++) {
        // Save state before playing the move
        uint8_t temp_board[sizeof(mcumax.board)];
        memcpy(temp_board, mcumax.board, sizeof(mcumax.board));
        uint8_t temp_en_passant = mcumax.en_passant_square;
        int32_t temp_score = mcumax.score;
        int32_t temp_npm = mcumax.non_pawn_material;
        
        // Play the move temporarily
        if (mcumax_play_move(valid_moves[i])) {
            // Check if this move puts own king in check (illegal move)
            bool puts_own_king_in_check = mcumax_is_in_check(side);
            
            // Restore state
            memcpy(mcumax.board, temp_board, sizeof(mcumax.board));
            mcumax.en_passant_square = temp_en_passant;
            mcumax.score = temp_score;
            mcumax.non_pawn_material = temp_npm;
            mcumax.current_side = side;
            
            // If this move is legal (does not put own king in check), it's not stalemate
            if (!puts_own_king_in_check) {
                // Restore full engine state
                memcpy(mcumax.board, board_backup, sizeof(mcumax.board));
                mcumax.current_side = current_side_backup;
                mcumax.en_passant_square = en_passant_backup;
                mcumax.score = score_backup;
                mcumax.non_pawn_material = npm_backup;
                return false;
            }
        } else {
            // The move could not be played, restore anyway
            memcpy(mcumax.board, temp_board, sizeof(mcumax.board));
            mcumax.en_passant_square = temp_en_passant;
            mcumax.score = temp_score;
            mcumax.non_pawn_material = temp_npm;
            mcumax.current_side = side;
        }
    }
    
    // 6. Restore full engine state
    memcpy(mcumax.board, board_backup, sizeof(mcumax.board));
    mcumax.current_side = current_side_backup;
    mcumax.en_passant_square = en_passant_backup;
    mcumax.score = score_backup;
    mcumax.non_pawn_material = npm_backup;
    
    // 7. If no legal move is available and the king is not in check, it's stalemate
    return true;
}

void mcumax_get_fen(char* fen_buffer, size_t buffer_size) {
    if (!fen_buffer || buffer_size < 100) {
        return; // Buffer too small for a complete FEN
    }
    
    char* ptr = fen_buffer;
    size_t remaining = buffer_size - 1; // Leave space for '\0'
    
    // 1. Piece positions (8 ranks separated by '/')
    for (int rank = 0; rank < 8; rank++) {
        int empty_count = 0;
        
        for (int file = 0; file < 8; file++) {
            mcumax_square square = rank * 16 + file;
            uint8_t piece_raw = mcumax.board[square];
            
            if (piece_raw == MCUMAX_EMPTY) {
                empty_count++;
            } else {
                // Write the number of empty squares if needed
                if (empty_count > 0) {
                    if (remaining < 1) return;
                    *ptr++ = '0' + empty_count;
                    remaining--;
                    empty_count = 0;
                }
                
                // Convert piece to FEN symbol
                char piece_char = '?';
                int piece_type = piece_raw & 0b111;
                bool is_white = (piece_raw & MCUMAX_BOARD_WHITE) != 0;
                
                switch (piece_type) {
                    case MCUMAX_PAWN_UPSTREAM:
                    case MCUMAX_PAWN_DOWNSTREAM:
                        piece_char = is_white ? 'P' : 'p';
                        break;
                    case MCUMAX_KNIGHT:
                        piece_char = is_white ? 'N' : 'n';
                        break;
                    case MCUMAX_KING:
                        piece_char = is_white ? 'K' : 'k';
                        break;
                    case MCUMAX_BISHOP:
                        piece_char = is_white ? 'B' : 'b';
                        break;
                    case MCUMAX_ROOK:
                        piece_char = is_white ? 'R' : 'r';
                        break;
                    case MCUMAX_QUEEN:
                        piece_char = is_white ? 'Q' : 'q';
                        break;
                }
                
                if (remaining < 1) return;
                *ptr++ = piece_char;
                remaining--;
            }
        }
        
        // Write the number of empty squares at the end of the rank if needed
        if (empty_count > 0) {
            if (remaining < 1) return;
            *ptr++ = '0' + empty_count;
            remaining--;
        }
        
        // Add '/' except for the last rank
        if (rank < 7) {
            if (remaining < 1) return;
            *ptr++ = '/';
            remaining--;
        }
    }
    
    // 2. Side to move
    if (remaining < 2) return;
    *ptr++ = ' ';
    *ptr++ = (mcumax.current_side == MCUMAX_BOARD_WHITE) ? 'w' : 'b';
    remaining -= 2;
    
    // 3. Castling rights
    if (remaining < 2) return;
    *ptr++ = ' ';
    remaining--;
    
    bool has_castling = false;
    
    // White king-side castling (K)
    if (!(mcumax.board[0x74] & MCUMAX_PIECE_MOVED) && !(mcumax.board[0x77] & MCUMAX_PIECE_MOVED)) {
        if (remaining < 1) return;
        *ptr++ = 'K';
        remaining--;
        has_castling = true;
    }
    
    // White queen-side castling (Q)
    if (!(mcumax.board[0x74] & MCUMAX_PIECE_MOVED) && !(mcumax.board[0x70] & MCUMAX_PIECE_MOVED)) {
        if (remaining < 1) return;
        *ptr++ = 'Q';
        remaining--;
        has_castling = true;
    }
    
    // Black king-side castling (k)
    if (!(mcumax.board[0x04] & MCUMAX_PIECE_MOVED) && !(mcumax.board[0x07] & MCUMAX_PIECE_MOVED)) {
        if (remaining < 1) return;
        *ptr++ = 'k';
        remaining--;
        has_castling = true;
    }
    
    // Black queen-side castling (q)
    if (!(mcumax.board[0x04] & MCUMAX_PIECE_MOVED) && !(mcumax.board[0x00] & MCUMAX_PIECE_MOVED)) {
        if (remaining < 1) return;
        *ptr++ = 'q';
        remaining--;
        has_castling = true;
    }
    
    // If no castling possible
    if (!has_castling) {
        if (remaining < 1) return;
        *ptr++ = '-';
        remaining--;
    }
    
    // 4. En passant square
    if (remaining < 2) return;
    *ptr++ = ' ';
    remaining--;
    
    if (mcumax.en_passant_square == MCUMAX_SQUARE_INVALID) {
        if (remaining < 1) return;
        *ptr++ = '-';
        remaining--;
    } else {
        // Convert square to algebraic notation
        int file = mcumax.en_passant_square & 0x0F;
        int rank = (mcumax.en_passant_square & 0xF0) >> 4;
        
        if (remaining < 2) return;
        *ptr++ = 'a' + file;
        *ptr++ = '8' - rank;
        remaining -= 2;
    }
    
    // 5. Halfmove clock (50-move rule) - Simplified to 0 as not tracked by engine
    if (remaining < 3) return;
    *ptr++ = ' ';
    *ptr++ = '0';
    remaining -= 2;
    
    // 6. Move number - Simplified to 1 as not tracked by engine
    if (remaining < 3) return;
    *ptr++ = ' ';
    *ptr++ = '1';
    remaining -= 2;
    
    // End the string
    *ptr = '\0';
}
