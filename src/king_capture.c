#include "mcu-max.h"

// Déclaration externe de la fonction de génération de pseudo-coups
uint32_t generate_pseudo_moves(uint8_t fromRank, uint8_t fromFile, mcumax_move *moves, uint32_t maxMoves);
#include <stdint.h>
#include <stdbool.h>

// Cette fonction suppose que le moteur est déjà positionné sur la bonne position
bool canKingBeCaptured(const void *board, uint8_t kingColor) {
    // Trouver la position du roi
    mcumax_square kingSquare = MCUMAX_SQUARE_INVALID;
    for (uint8_t rank = 0; rank < 8; ++rank) {
        for (uint8_t file = 0; file < 8; ++file) {
            mcumax_square sq = (rank << 4) | file;
            mcumax_piece p = mcumax_get_piece(sq);
            if ((p & 0x7) == MCUMAX_KING && ((p & MCUMAX_BLACK) ? 1 : 0) == kingColor) {
                kingSquare = sq;
                break;
            }
        }
        if (kingSquare != MCUMAX_SQUARE_INVALID) break;
    }
    if (kingSquare == MCUMAX_SQUARE_INVALID) return false;

    // Parcourir toutes les pièces adverses et voir si elles peuvent capturer le roi
    for (uint8_t rank = 0; rank < 8; ++rank) {
        for (uint8_t file = 0; file < 8; ++file) {
            mcumax_square sq = (rank << 4) | file;
            mcumax_piece p = mcumax_get_piece(sq);
            if ((p & 0x7) != MCUMAX_EMPTY && ((p & MCUMAX_BLACK) ? 1 : 0) != kingColor) {
                mcumax_move moves[32];
                uint32_t nMoves = generate_pseudo_moves(rank, file, moves, 32);
                for (uint32_t i = 0; i < nMoves; ++i) {
                    if (moves[i].to == kingSquare) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}
