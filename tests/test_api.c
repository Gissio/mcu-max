/*
 * mcu-max API tests
 *
 * Build with -fsanitize=address,undefined to also catch memory errors.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mcu-max.h"

#define CHECK(condition)                                       \
    do                                                         \
    {                                                          \
        if (!(condition))                                      \
        {                                                      \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            exit(1);                                           \
        }                                                      \
    } while (0)

#define MOVE(from_file, from_rank, to_file, to_rank)  \
    ((mcumax_move){MCUMAX_SQUARE(from_file, from_rank), \
                   MCUMAX_SQUARE(to_file, to_rank)})

static uint32_t callback_count;
static uint32_t callback_stop_at;

static void callback(void *userdata)
{
    CHECK(userdata == &callback_count);

    if (++callback_count == callback_stop_at)
        mcumax_stop_search();
}

static void test_squares(void)
{
    CHECK(MCUMAX_SQUARE(4, 1) == 0x64);
    CHECK(MCUMAX_SQUARE(0, 7) == 0x00);
    CHECK(MCUMAX_SQUARE(7, 0) == 0x77);
    CHECK(MCUMAX_SQUARE_FILE(0x64) == 4);
    CHECK(MCUMAX_SQUARE_RANK(0x64) == 1);
}

static void test_pieces(void)
{
    mcumax_init();

    CHECK(mcumax_get_piece(MCUMAX_SQUARE(4, 0)) == (MCUMAX_KING | MCUMAX_WHITE));
    CHECK(mcumax_get_piece(MCUMAX_SQUARE(3, 0)) == (MCUMAX_QUEEN | MCUMAX_WHITE));
    CHECK(mcumax_get_piece(MCUMAX_SQUARE(0, 0)) == (MCUMAX_ROOK | MCUMAX_WHITE));
    CHECK(mcumax_get_piece(MCUMAX_SQUARE(2, 0)) == (MCUMAX_BISHOP | MCUMAX_WHITE));
    CHECK(mcumax_get_piece(MCUMAX_SQUARE(1, 0)) == (MCUMAX_KNIGHT | MCUMAX_WHITE));
    CHECK(mcumax_get_piece(MCUMAX_SQUARE(0, 1)) == (MCUMAX_PAWN | MCUMAX_WHITE));
    CHECK(mcumax_get_piece(MCUMAX_SQUARE(7, 6)) == (MCUMAX_PAWN | MCUMAX_BLACK));
    CHECK(mcumax_get_piece(MCUMAX_SQUARE(3, 7)) == (MCUMAX_QUEEN | MCUMAX_BLACK));
    CHECK(mcumax_get_piece(MCUMAX_SQUARE(4, 7)) == (MCUMAX_KING | MCUMAX_BLACK));
    CHECK(mcumax_get_piece(MCUMAX_SQUARE(4, 4)) == MCUMAX_EMPTY);

    CHECK(MCUMAX_PIECE_TYPE(MCUMAX_KNIGHT | MCUMAX_BLACK) == MCUMAX_KNIGHT);
    CHECK(MCUMAX_PIECE_COLOR(MCUMAX_KNIGHT | MCUMAX_BLACK) == MCUMAX_BLACK);

    // Off-board and invalid squares
    CHECK(mcumax_get_piece(0x08) == MCUMAX_EMPTY);
    CHECK(mcumax_get_piece(MCUMAX_SQUARE_INVALID) == MCUMAX_EMPTY);
}

static void test_game(void)
{
    mcumax_init();

    CHECK(mcumax_get_current_side() == MCUMAX_WHITE);
    CHECK(mcumax_search_valid_moves(NULL, 0) == 20);
    CHECK(mcumax_get_game_state() == MCUMAX_GAME_ONGOING);

    // Valid moves larger than buffer: count is still returned
    mcumax_move moves[4];
    CHECK(mcumax_search_valid_moves(moves, 4) == 20);

    // Fool's mate
    CHECK(mcumax_play_move(MOVE(5, 1, 5, 2)));
    CHECK(mcumax_get_current_side() == MCUMAX_BLACK);
    CHECK(mcumax_play_move(MOVE(4, 6, 4, 4)));
    CHECK(!mcumax_play_move(MOVE(4, 1, 4, 4))); // Blocked
    CHECK(!mcumax_play_move(MCUMAX_MOVE_INVALID));
    CHECK(mcumax_play_move(MOVE(6, 1, 6, 3)));
    CHECK(mcumax_play_move(MOVE(3, 7, 7, 3)));
    CHECK(mcumax_get_game_state() == MCUMAX_GAME_CHECKMATE);
    CHECK(mcumax_search_valid_moves(NULL, 0) == 0);
    CHECK(mcumax_search_best_move(10000, 10).from == MCUMAX_SQUARE_INVALID);
}

static void test_fen(void)
{
    char fen_string[MCUMAX_FEN_SIZE];

    mcumax_set_fen_position("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");
    CHECK(mcumax_get_game_state() == MCUMAX_GAME_STALEMATE);

    mcumax_set_fen_position("4k3/8/8/8/8/8/4q3/4K3 w - - 0 1");
    CHECK(mcumax_get_game_state() == MCUMAX_GAME_CHECK);

    // Engine finds mate in one
    mcumax_set_fen_position("6k1/5ppp/8/8/8/8/5PPP/3R2K1 w - - 0 1");
    mcumax_move move = mcumax_search_best_move(10000, 10);
    CHECK((move.from == MCUMAX_SQUARE(3, 0)) && (move.to == MCUMAX_SQUARE(3, 7)));

    // FEN output
    mcumax_init();
    mcumax_get_fen_position(fen_string);
    CHECK(!strcmp(fen_string,
                  "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"));
    CHECK(mcumax_play_move(MOVE(4, 1, 4, 3)));
    mcumax_get_fen_position(fen_string);
    CHECK(!strcmp(fen_string,
                  "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1"));

    // Malformed FEN strings must not crash
    static const char *const bad_fen_strings[] = {
        "",
        "////////////////////",
        "8888888888888888888888/8/8",
        "pppppppppppppppppppppppppp/RRRRRRRRRRRRRR",
        "k w KQkq e9 5 5",
        "8/8/8/8/8/8/8/8 x - 3",
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq 3 0 1",
    };
    for (uint32_t i = 0; i < sizeof(bad_fen_strings) / sizeof(*bad_fen_strings); i++)
    {
        mcumax_set_fen_position(bad_fen_strings[i]);
        mcumax_search_valid_moves(NULL, 0);
        mcumax_get_game_state();
        mcumax_get_fen_position(fen_string);
    }
}

static void test_callback(void)
{
    mcumax_set_callback(callback, &callback_count);

    // Callback is called
    mcumax_init();
    callback_count = 0;
    callback_stop_at = 0;
    CHECK(mcumax_search_best_move(1000, 10).from != MCUMAX_SQUARE_INVALID);
    CHECK(callback_count > 0);

    // Stopped search returns no move
    mcumax_init();
    callback_count = 0;
    callback_stop_at = 100;
    CHECK(mcumax_search_best_move(1000000, 10).from == MCUMAX_SQUARE_INVALID);

    mcumax_set_callback(NULL, NULL);
}

int main(void)
{
    test_squares();
    test_pieces();
    test_game();
    test_fen();
    test_callback();

    printf("API tests passed\n");

    return 0;
}
