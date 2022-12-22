/*
 * mcu-max UCI chess interface
 *
 * (C) 2022 Gissio
 *
 * License: MIT
 */

#include <stdio.h>
#include <string.h>

#include "mcu-max.h"

#define MAIN_VALID_MOVES_NUM 512

void print_board()
{
    const char *symbols = ".?pnkbrq?P?NKBRQ";

    printf("  +-----------------+\n");

    for (int y = 0; y < 8; y++)
    {
        printf("%d | ", 8 - y);
        for (int x = 0; x < 8; x++)
            printf("%c ", symbols[mcumax_get_piece(0x10 * y + x) & 0xf]);
        printf("|\n");
    }

    printf("  +-----------------+\n");
}

mcumax_square get_square(char *s)
{
    mcumax_square rank = s[0] - 'a';
    if (rank > 7)
        return 0x80;

    mcumax_square file = '8' - s[1];
    if (file > 7)
        return 0x80;

    return 0x10 * file + rank;
}

bool is_valid_square(char *s)
{
    return (get_square(s) != MCUMAX_INVALID);
}

void print_square(mcumax_square square)
{
    printf("%c%c",
           'a' + ((square & 0x07) >> 0),
           '1' + 7 - ((square & 0x70) >> 4));
}

bool is_valid_move(char *s)
{
    return is_valid_square(s) && is_valid_square(s + 2);
}

void print_move(mcumax_move move)
{
    if ((move.from == MCUMAX_INVALID) ||
        (move.to == MCUMAX_INVALID))
        printf("(none)");
    else
    {
        print_square(move.from);
        print_square(move.to);
    }
}

int main()
{
    mcumax_reset();

    // UCI command loop
    int i = 0;
    while (1)
    {
        char line[65536];
        fgets(line, sizeof(line), stdin);

        char *token = strtok(line, " \n");

        if (!token)
            continue;

        if (!strcmp(token, "uci"))
        {
            printf("id name " MCUMAX_NAME "\n");
            printf("id author " MCUMAX_AUTHOR "\n");
            printf("uciok\n");
        }
        else if (!strcmp(token, "uci") || !strcmp(token, "ucinewgame"))
        {
            mcumax_reset();
        }
        else if (!strcmp(token, "isready"))
        {
            printf("readyok\n");
        }
        else if (!strcmp(token, "d"))
        {
            print_board();
        }
        else if (!strcmp(token, "l"))
        {
            mcumax_move valid_moves[MAIN_VALID_MOVES_NUM];
            int valid_moves_num = mcumax_get_valid_moves(valid_moves, MAIN_VALID_MOVES_NUM);

            for (int i = 0; i < valid_moves_num; i++)
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

            while (token = strtok(NULL, " \n"))
            {
                if (fen_index)
                {
                    strcat(fen_string, token);
                    strcat(fen_string, " ");

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
                        mcumax_reset();
                    else if (!strcmp(token, "fen"))
                    {
                        fen_index = 1;
                        strcpy(fen_string, "");
                    }
                    else if (is_valid_move(token))
                    {
                        mcumax_move move = {
                            get_square(token + 0),
                            get_square(token + 2),
                        };
                        mcumax_play_move(move);
                    }
                }
            }
        }
        else if (!strcmp(token, "go"))
        {
            int nodes_limit = 1000000;
            mcumax_move move;
            mcumax_play_best_move(nodes_limit, &move);

            printf("bestmove ");
            print_move(move);
            printf("\n");
        }
        else if (!strcmp(token, "quit"))
            break;
        else
            printf("Unknown command: %s\n", token);

        fflush(stdout);
    }
}
