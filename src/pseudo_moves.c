#include "mcu-max.h"
#include <stdint.h>
#include <stdbool.h>

// Génère les pseudo-coups (même illégaux) pour une pièce donnée
// Se concentre sur les coups de capture, y compris sur le roi
// Ne gère pas les promotions, roques, etc.
uint32_t generate_pseudo_moves(uint8_t fromRank, uint8_t fromFile, mcumax_move *moves, uint32_t maxMoves) {
    mcumax_square from = (fromRank << 4) | fromFile;
    mcumax_piece piece = mcumax_get_piece(from);
    uint8_t color = (piece & MCUMAX_BLACK) ? 1 : 0;
    uint32_t n = 0;

    // Directions pour chaque type de pièce
    static const int knightDirs[8][2] = {
        {1,2},{2,1},{-1,2},{-2,1},{1,-2},{2,-1},{-1,-2},{-2,-1}
    };
    static const int kingDirs[8][2] = {
        {1,0},{0,1},{-1,0},{0,-1},{1,1},{-1,1},{1,-1},{-1,-1}
    };
    static const int bishopDirs[4][2] = {
        {1,1},{-1,1},{1,-1},{-1,-1}
    };
    static const int rookDirs[4][2] = {
        {1,0},{-1,0},{0,1},{0,-1}
    };
    static const int queenDirs[8][2] = {
        {1,0},{0,1},{-1,0},{0,-1},{1,1},{-1,1},{1,-1},{-1,-1}
    };

    // Pion
    if ((piece & 0x7) == MCUMAX_PAWN_UPSTREAM || (piece & 0x7) == MCUMAX_PAWN_DOWNSTREAM) {
        int dir = ((piece & 0x7) == MCUMAX_PAWN_UPSTREAM) ? 1 : -1;
        for (int dx = -1; dx <= 1; dx += 2) {
            int toRank = fromRank + dir;
            int toFile = fromFile + dx;
            if (toRank >= 0 && toRank < 8 && toFile >= 0 && toFile < 8) {
                mcumax_square to = (toRank << 4) | toFile;
                mcumax_piece target = mcumax_get_piece(to);
                if ((target & 0x7) != MCUMAX_EMPTY && ((target & MCUMAX_BLACK) ? 1 : 0) != color) {
                    if (n < maxMoves) {
                        moves[n++] = (mcumax_move){from, to};
                    }
                }
            }
        }
    }
    // Cavalier
    else if ((piece & 0x7) == MCUMAX_KNIGHT) {
        for (int i = 0; i < 8; ++i) {
            int toRank = fromRank + knightDirs[i][0];
            int toFile = fromFile + knightDirs[i][1];
            if (toRank >= 0 && toRank < 8 && toFile >= 0 && toFile < 8) {
                mcumax_square to = (toRank << 4) | toFile;
                mcumax_piece target = mcumax_get_piece(to);
                if ((target & 0x7) != MCUMAX_EMPTY && ((target & MCUMAX_BLACK) ? 1 : 0) != color) {
                    if (n < maxMoves) {
                        moves[n++] = (mcumax_move){from, to};
                    }
                }
            }
        }
    }
    // Roi
    else if ((piece & 0x7) == MCUMAX_KING) {
        for (int i = 0; i < 8; ++i) {
            int toRank = fromRank + kingDirs[i][0];
            int toFile = fromFile + kingDirs[i][1];
            if (toRank >= 0 && toRank < 8 && toFile >= 0 && toFile < 8) {
                mcumax_square to = (toRank << 4) | toFile;
                mcumax_piece target = mcumax_get_piece(to);
                if ((target & 0x7) != MCUMAX_EMPTY && ((target & MCUMAX_BLACK) ? 1 : 0) != color) {
                    if (n < maxMoves) {
                        moves[n++] = (mcumax_move){from, to};
                    }
                }
            }
        }
    }
    // Fou
    else if ((piece & 0x7) == MCUMAX_BISHOP) {
        for (int d = 0; d < 4; ++d) {
            for (int dist = 1; dist < 8; ++dist) {
                int toRank = fromRank + bishopDirs[d][0] * dist;
                int toFile = fromFile + bishopDirs[d][1] * dist;
                if (toRank < 0 || toRank >= 8 || toFile < 0 || toFile >= 8) break;
                mcumax_square to = (toRank << 4) | toFile;
                mcumax_piece target = mcumax_get_piece(to);
                if ((target & 0x7) != MCUMAX_EMPTY) {
                    if (((target & MCUMAX_BLACK) ? 1 : 0) != color) {
                        if (n < maxMoves) moves[n++] = (mcumax_move){from, to};
                    }
                    break;
                }
            }
        }
    }
    // Tour
    else if ((piece & 0x7) == MCUMAX_ROOK) {
        for (int d = 0; d < 4; ++d) {
            for (int dist = 1; dist < 8; ++dist) {
                int toRank = fromRank + rookDirs[d][0] * dist;
                int toFile = fromFile + rookDirs[d][1] * dist;
                if (toRank < 0 || toRank >= 8 || toFile < 0 || toFile >= 8) break;
                mcumax_square to = (toRank << 4) | toFile;
                mcumax_piece target = mcumax_get_piece(to);
                if ((target & 0x7) != MCUMAX_EMPTY) {
                    if (((target & MCUMAX_BLACK) ? 1 : 0) != color) {
                        if (n < maxMoves) moves[n++] = (mcumax_move){from, to};
                    }
                    break;
                }
            }
        }
    }
    // Dame
    else if ((piece & 0x7) == MCUMAX_QUEEN) {
        for (int d = 0; d < 8; ++d) {
            for (int dist = 1; dist < 8; ++dist) {
                int toRank = fromRank + queenDirs[d][0] * dist;
                int toFile = fromFile + queenDirs[d][1] * dist;
                if (toRank < 0 || toRank >= 8 || toFile < 0 || toFile >= 8) break;
                mcumax_square to = (toRank << 4) | toFile;
                mcumax_piece target = mcumax_get_piece(to);
                if ((target & 0x7) != MCUMAX_EMPTY) {
                    if (((target & MCUMAX_BLACK) ? 1 : 0) != color) {
                        if (n < maxMoves) moves[n++] = (mcumax_move){from, to};
                    }
                    break;
                }
            }
        }
    }
    return n;
}
