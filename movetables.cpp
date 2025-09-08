#include "movetables.h"

void MoveTables::init() {
    generateKingMoves();
    generateKnightMoves();
    generatePawnMoves();
    generateSlidingPieceMoves();
    generateZobristTables();
}

void MoveTables::generateZobristTables() {
    U64 seed = 1070372; // fixed seed for reproducibility

    // piece-square hash values
    for (int piece = 0; piece < 12; piece++) {
        for (int square = 0; square < 64; square++) {
            zobristTable[piece][square] = randomU64(seed);
        }
    }
    // side to move hash
    zobristSideToMove = randomU64(seed);

    // castling rights hash (16 combos)
    for (int i = 0; i < 16; i++) {
        zobristCastling[i] = randomU64(seed);
    }

    // en passant files hash (8 files)
    for (int i = 0; i < 8; i++) {
        zobristEnPassant[i] = randomU64(seed);
    }
}

U64 MoveTables::randomU64(U64& seed) {
    seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
    return seed;
}

void MoveTables::generateKingMoves() {
    int setKingMoves[8][2] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1},  {-1, -1}, {-1, 1} };
    
    for (int square = 0; square < 64; square++) {
        U64 kingMoves_i = 0ULL;
        int col = square % 8, row = square / 8;
        
        for (int i = 0; i < 8; i++) {
            updateBitboard(kingMoves_i, row, col, setKingMoves[i]);
        }
        
        kingBB[square] = kingMoves_i;
    }
}

void MoveTables::generateKnightMoves() {
    int setKnightMoves[8][2] = { {2, 1}, {2, -1}, {-2, 1}, {-2, -1}, {1, 2}, {1, -2}, {-1, 2}, {-1, -2} };
    
    for (int square = 0; square < 64; square++) {
        U64 knightMoves_i = 0ULL;
        int col = square % 8, row = square / 8;
        
        for (int i = 0; i < 8; i++) {
            updateBitboard(knightMoves_i, row, col, setKnightMoves[i]);
        }
        
        knightBB[square] = knightMoves_i;
    }
}

void MoveTables::generatePawnMoves() {
    for (int square = 0; square < 64; square++) {
        int col = square % 8, row = square / 8;
        U64 whitePawnMoves_i = 0ULL, blackPawnMoves_i = 0ULL;
        U64 whitePawnMovesCaptures_i = 0ULL, blackPawnMovesCaptures_i = 0ULL;
        
        updatePawnMoves(whitePawnMoves_i, blackPawnMoves_i, row, col);
        updatePawnCaptures(whitePawnMovesCaptures_i, blackPawnMovesCaptures_i, row, col);
        
        pawnMovesBB[nWhite][square] = whitePawnMoves_i;
        pawnMovesBB[nBlack][square] = blackPawnMoves_i;
        pawnMovesCapturesBB[nWhite][square] = whitePawnMovesCaptures_i;
        pawnMovesCapturesBB[nBlack][square] = blackPawnMovesCaptures_i;
    }
}

void MoveTables::generateSlidingPieceMoves() {
    for (int square = 0; square < 64; square++) {
        int col = square % 8, row = square / 8;
        
        U64 rookMoves_i = 0ULL, bishopMoves_i = 0ULL;
        
        for (int c = 0; c < 8; c++) rookMoves_i |= 1ULL << (row * 8 + c);
        for (int r = 0; r < 8; r++) rookMoves_i ^= 1ULL << (r * 8 + col);

        for (int i = 1; i < 8; i++) {
            if (row - i >= 0 && col - i >= 0) bishopMoves_i |= 1ULL << ((row - i) * 8 + col - i);
            if (row + i < 8 && col - i >= 0) bishopMoves_i |= 1ULL << ((row + i) * 8 + col - i);
            if (row - i >= 0 && col + i < 8) bishopMoves_i |= 1ULL << ((row - i) * 8 + col + i);
            if (row + i < 8 && col + i < 8) bishopMoves_i |= 1ULL << ((row + i) * 8 + col + i);
        }
    }
}

void MoveTables::updatePawnMoves(U64 &whitePawnMoves, U64 &blackPawnMoves, int row, int col) {
    // White pawns move up (to higher rows)
    if (row + 1 < 8) whitePawnMoves |= 1ULL << ((row + 1) * 8 + col);
    if (row == 1)    whitePawnMoves |= 1ULL << ((row + 2) * 8 + col);
    // Black pawns move down (to lower rows)
    if (row - 1 >= 0) blackPawnMoves |= 1ULL << ((row - 1) * 8 + col);
    if (row == 6)     blackPawnMoves |= 1ULL << ((row - 2) * 8 + col);
}

void MoveTables::updatePawnCaptures(U64 &whiteCaptures, U64 &blackCaptures, int row, int col) {
    // White captures up
    if (col + 1 < 8 && row + 1 < 8) whiteCaptures |= 1ULL << ((row + 1) * 8 + col + 1);
    if (col - 1 >= 0 && row + 1 < 8) whiteCaptures |= 1ULL << ((row + 1) * 8 + col - 1);
    // Black captures down
    if (col - 1 >= 0 && row - 1 >= 0) blackCaptures |= 1ULL << ((row - 1) * 8 + col - 1);
    if (col + 1 < 8 && row - 1 >= 0) blackCaptures |= 1ULL << ((row - 1) * 8 + col + 1);
}


void MoveTables::updateBitboard(U64 &bitboard, int row, int col, int move[2]) {
    int newRow = row + move[0], newCol = col + move[1];
    if (newRow >= 0 && newRow < 8 && newCol >= 0 && newCol < 8) {
        bitboard |= 1ULL << (newRow * 8 + newCol);
    }
}
