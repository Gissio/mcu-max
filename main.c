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

void print_move(mcumax_square move_from, mcumax_square move_to)
{
    if ((move_from == MCUMAX_INVALID) ||
        (move_to == MCUMAX_INVALID))
        printf("(none)");
    else
    {
        print_square(move_from);
        print_square(move_to);
    }
}

void logLine(char *line)
{
    FILE *fp = fopen("C:\\Users\\mressl\\log.txt", "at");
    fputs(line, fp);
    fclose(fp);
}

int main()
{
    mcumax_reset();

    // UCI command loop
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
        else if (!strcmp(token, "uci") || !strcmp(token, "ucinewgame") )
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
            mcumax_square valid_moves[512];
            mcumax_get_valid_moves(valid_moves, sizeof(valid_moves));

            mcumax_square move_from;
            mcumax_square *valid_moves_p = valid_moves;
            while ((move_from = *valid_moves_p++) != MCUMAX_INVALID)
            {
                mcumax_square move_to;
                while ((move_to = *valid_moves_p++) != MCUMAX_INVALID)
                {
                    print_move(move_from, move_to);
                    printf(" ");
                }
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
                        mcumax_play_move(get_square(token + 0), get_square(token + 2));
                    }
                }
            }
        }
        else if (!strcmp(token, "go"))
        {
            int nodes_limit = 1000000;
            mcumax_square move_from, move_to;
            mcumax_play_best_move(nodes_limit, &move_from, &move_to);

            printf("bestmove ");
            print_move(move_from, move_to);
            printf("\n");
        }
        else if (!strcmp(token, "quit"))
            break;
        else
            printf("Unknown command: %s\n", token);

        fflush(stdout);
    }
}
