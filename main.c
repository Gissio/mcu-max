/*
 * mcu-max UCI chess interface
 *
 * (C) 2022 Gissio
 *
 * License: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mcu-max.h"

#define MAIN_VALID_MOVES_NUM 512

void print_board()
{
    const char *symbols = ".PPNKBRQ.ppnkbrq";

    printf("  +-----------------+\n");

    for (uint32_t y = 0; y < 8; y++)
    {
        printf("%d | ", 8 - y);
        for (uint32_t x = 0; x < 8; x++)
            printf("%c ", symbols[mcumax_get_piece(0x10 * y + x)]);
        printf("|\n");
    }

    printf("  +-----------------+\n");
    printf("    a b c d e f g h\n");

    printf("\n");
}

mcumax_square get_square(char *s)
{
    mcumax_square rank = s[0] - 'a';
    if (rank > 7)
        return MCUMAX_SQUARE_INVALID;

    mcumax_square file = '8' - s[1];
    if (file > 7)
        return MCUMAX_SQUARE_INVALID;

    return 0x10 * file + rank;
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
           'a' + ((square & 0x07) >> 0),
           '1' + 7 - ((square & 0x70) >> 4));
}

void print_move(mcumax_move move)
{
    if ((move.from == MCUMAX_SQUARE_INVALID) ||
        (move.to == MCUMAX_SQUARE_INVALID))
        printf("(none)");
    else
    {
        print_square(move.from);
        print_square(move.to);
    }
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
    else if (!strcmp(token, "uci") ||
             !strcmp(token, "ucinewgame"))
        mcumax_init();
    else if (!strcmp(token, "isready"))
        printf("readyok\n");
    else if (!strcmp(token, "d"))
        print_board();
    else if (!strcmp(token, "l"))
    {
        mcumax_move valid_moves[MAIN_VALID_MOVES_NUM];
        uint32_t valid_moves_num = mcumax_search_valid_moves(valid_moves, MAIN_VALID_MOVES_NUM);

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
    }
    else if (!strcmp(token, "go"))
    {
        mcumax_move move = mcumax_search_best_move(1, 30);
        mcumax_play_move(move);

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

int main()
{
    mcumax_init();

    while (true)
    {
        fflush(stdout);

        char line[65536];
        fgets(line, sizeof(line), stdin);

        if (send_uci_command(line))
            break;
    }
}
