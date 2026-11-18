/*
 * mcu-max UCI chess interface example
 *
 * (C) 2022-2024 Gissio
 *
 * License: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mcu-max.h"

#define MAIN_VALID_MOVES_NUM 512

void print_board(void)
{
    const char *symbols = ".PNBRQK?.pnbrqk?";

    printf("  +-----------------+\n");

    for (uint32_t y = 0; y < 8; y++)
    {
        printf("%d | ", (int)(8 - y));
        for (uint32_t x = 0; x < 8; x++)
            printf("%c ", symbols[mcumax_get_piece(MCUMAX_SQUARE(x, 7 - y))]);
        printf("|\n");
    }

    printf("  +-----------------+\n");
    printf("    a b c d e f g h\n");

    printf("\n");
}

mcumax_square get_square(char *s)
{
    uint8_t file = s[0] - 'a';
    uint8_t rank = s[1] - '1';
    if ((file > 7) || (rank > 7))
        return MCUMAX_SQUARE_INVALID;

    return MCUMAX_SQUARE(file, rank);
}

bool is_square_valid(char *s)
{
    return (get_square(s) != MCUMAX_SQUARE_INVALID);
}

bool is_move_valid(char *s)
{
    return is_square_valid(s) && is_square_valid(s + 2);
}

void print_square(mcumax_square square)
{
    printf("%c%c",
           'a' + MCUMAX_SQUARE_FILE(square),
           '1' + MCUMAX_SQUARE_RANK(square));
}

void print_move(mcumax_move move)
{
    if ((move.from == MCUMAX_SQUARE_INVALID) ||
        (move.to == MCUMAX_SQUARE_INVALID))
        printf("0000");
    else
    {
        print_square(move.from);
        print_square(move.to);

        // Promotion (always to queen)
        if ((MCUMAX_PIECE_TYPE(mcumax_get_piece(move.from)) == MCUMAX_PAWN) &&
            ((MCUMAX_SQUARE_RANK(move.to) == 0) ||
             (MCUMAX_SQUARE_RANK(move.to) == 7)))
            printf("q");
    }
}

double get_time(void)
{
    return (double)clock() / CLOCKS_PER_SEC;
}

// Searches the best move within a time budget (seconds), doubling the
// node limit while the next search is expected to fit
mcumax_move search_timed(double budget, uint32_t depth_max)
{
    double start_time = get_time();
    mcumax_move move = MCUMAX_MOVE_INVALID;

    for (uint32_t node_max = 1000; node_max < (1UL << 30); node_max *= 2)
    {
        double search_start_time = get_time();

        mcumax_move new_move = mcumax_search_best_move(node_max, depth_max);
        if (new_move.from == MCUMAX_SQUARE_INVALID)
            break;
        move = new_move;

        // Next search takes about twice as long
        double now = get_time();
        if ((now - start_time) + 2.5 * (now - search_start_time) > budget)
            break;
    }

    return move;
}

bool send_uci_command(char *line)
{
    char *token = strtok(line, " \n");

    if (!token)
        return false;

    if (!strcmp(token, "uci"))
    {
        printf("id name " MCUMAX_ID "\n");
        printf("id author " MCUMAX_AUTHOR "\n");
        printf("uciok\n");
    }
    else if (!strcmp(token, "ucinewgame"))
        mcumax_init();
    else if (!strcmp(token, "isready"))
        printf("readyok\n");
    else if (!strcmp(token, "d"))
    {
        static const char *const states[] = {
            "", "Check", "Stalemate", "Checkmate"};

        char fen_string[MCUMAX_FEN_SIZE];
        mcumax_get_fen_position(fen_string);

        print_board();
        printf("Fen: %s\n", fen_string);
        printf("%s\n", states[mcumax_get_game_state()]);
    }
    else if (!strcmp(token, "l"))
    {
        mcumax_move valid_moves[MAIN_VALID_MOVES_NUM];
        uint32_t valid_moves_num = mcumax_search_valid_moves(valid_moves, MAIN_VALID_MOVES_NUM);

        if (valid_moves_num > MAIN_VALID_MOVES_NUM)
            valid_moves_num = MAIN_VALID_MOVES_NUM;

        for (uint32_t i = 0; i < valid_moves_num; i++)
        {
            print_move(valid_moves[i]);
            printf(" ");
        }
        printf("\n");
    }
    else if (!strcmp(token, "position"))
    {
        int fen_index = 0;
        char fen_string[256];

        while ((token = strtok(NULL, " \n")))
        {
            if (fen_index && !strcmp(token, "moves"))
            {
                // FEN with omitted trailing fields
                mcumax_set_fen_position(fen_string);

                fen_index = 0;
            }
            else if (fen_index)
            {
                if (strlen(fen_string) + strlen(token) + 2 <= sizeof(fen_string))
                {
                    strcat(fen_string, token);
                    strcat(fen_string, " ");
                }

                fen_index++;
                if (fen_index > 6)
                {
                    mcumax_set_fen_position(fen_string);

                    fen_index = 0;
                }
            }
            else
            {
                if (!strcmp(token, "startpos"))
                    mcumax_init();
                else if (!strcmp(token, "fen"))
                {
                    fen_index = 1;
                    strcpy(fen_string, "");
                }
                else if (is_move_valid(token))
                {
                    mcumax_play_move((mcumax_move){
                        get_square(token + 0),
                        get_square(token + 2),
                    });
                }
            }
        }

        // FEN at end of line with omitted trailing fields
        if (fen_index)
            mcumax_set_fen_position(fen_string);
    }
    else if (!strcmp(token, "go"))
    {
        // Limits: go [nodes N] [depth N] [movetime ms]
        //            [wtime ms] [btime ms] [winc ms] [binc ms]
        uint32_t node_max = 1000000;
        uint32_t depth_max = 30;
        long movetime = -1, time_left = -1, increment = 0;
        bool is_white = (mcumax_get_current_side() == MCUMAX_WHITE);

        char *name;
        while ((name = strtok(NULL, " \n")))
        {
            char *value = strtok(NULL, " \n");
            if (!value)
                break;

            long n = atol(value);
            if (!strcmp(name, "nodes"))
                node_max = n;
            else if (!strcmp(name, "depth"))
                depth_max = (n < 1) ? 1 : (n > 90) ? 90 : n; // mcu-max: <= 90
            else if (!strcmp(name, "movetime"))
                movetime = n;
            else if (!strcmp(name, is_white ? "wtime" : "btime"))
                time_left = n;
            else if (!strcmp(name, is_white ? "winc" : "binc"))
                increment = n;
        }

        // Clock: use a fraction of the remaining time
        if ((movetime < 0) && (time_left >= 0))
            movetime = time_left / 30 + increment / 2;

        mcumax_move move = (movetime >= 0)
                               ? search_timed(movetime / 1000.0, depth_max)
                               : mcumax_search_best_move(node_max, depth_max);

        printf("bestmove ");
        print_move(move);
        printf("\n");
    }
    else if (!strcmp(token, "quit"))
        return true;
    else
        printf("Unknown command: %s\n", token);

    return false;
}

int main(void)
{
    mcumax_init();

    while (true)
    {
        fflush(stdout);

        static char line[65536];
        if (!fgets(line, sizeof(line), stdin))
            break;

        if (send_uci_command(line))
            break;
    }
}
