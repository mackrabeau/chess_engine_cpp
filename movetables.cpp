#include "movetables.h"
#include "bitboard.h"

void MoveTables::init() {
    generateKingMoves();
    generateKnightMoves();
    generatePawnMoves();
    MagicBitboard::instance().init();
    generateZobristTables();
    generateRays();
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

void MoveTables::generateRays() {
    for (int from = 0; from < 64; from++) {
        for (int to = 0; to < 64; to++) {

            int fromRow = from / 8, fromCol = from % 8;
            int toRow = to / 8, toCol = to % 8;

            int rowDiff = toRow - fromRow;
            int colDiff = toCol - fromCol;

            bool sameRank = fromRow == toRow;
            bool sameFile = fromCol == toCol;
            bool sameDiagonal = abs(fromRow - toRow) == abs(fromCol - toCol);

            if (!sameRank && !sameFile && !sameDiagonal) {
                rays[from][to] = 0ULL;
                continue;
            }

            int rowDir = (rowDiff == 0) ? 0 : (rowDiff > 0) ? 1 : -1;
            int colDir = (colDiff == 0) ? 0 : (colDiff > 0) ? 1 : -1;

            U64 ray = 0ULL;
            int r = fromRow + rowDir;
            int c = fromCol + colDir;

            while (r >= 0 && r < 8 && c >= 0 && c < 8) {
                int square = r * 8 + c;
                ray |= 1ULL << square;
                if (square == to) break;
                r += rowDir;
                c += colDir;
            }
            // ray |= 1ULL << from; // include starting square in ray
            rays[from][to] = ray;
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
