#ifndef MOVETABLES_H
#define MOVETABLES_H

#include "types.h"
#include <cstdlib>


class MoveTables {
public:
    U64 kingBB[64];
    U64 knightBB[64];
    U64 pawnMovesBB[2][64];
    U64 pawnMovesCapturesBB[2][64];

    U64 zobristTable[12][64];   // hash 12 piece types (6 for each color) and 64 squares
    U64 zobristSideToMove;      // Zobrist hash for side to move
    U64 zobristCastling[16];    // hash 16 possible castling rights (4 for each color)
    U64 zobristEnPassant[8];    // hash 8 possible en passant files

    static MoveTables& instance() {
        static MoveTables instance;
        return instance;
    }

    void init();

private:
    MoveTables() { init(); } // private constructor to enforce singleton pattern
    MoveTables(const MoveTables&) = delete; // prevent copying
    MoveTables& operator=(const MoveTables&) = delete; // prevent assignment

    U64 randomU64(U64& seed);

    void generateKingMoves();
    void generateKnightMoves();
    void generatePawnMoves();
    void generateZobristTables();

    
    void generateSlidingPieceMoves();

    void updateBitboard(U64 &bitboard, int row, int col, int move[2]);
    void updatePawnMoves(U64 &whitePawnMoves, U64 &blackPawnMoves, int row, int col);
    void updatePawnCaptures(U64 &whiteCaptures, U64 &blackCaptures, int row, int col);

};

#endif // MOVETABLES_H