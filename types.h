#ifndef TYPES_H
#define TYPES_H

#include <cstdint>

typedef uint64_t U64;
typedef uint16_t U16;
typedef int16_t I16; // signed 16-bit integer
typedef uint32_t U32;
typedef uint8_t U8;

enum enumPiece {
    nBlack = 0,     // all black pieces
    nWhite = 1,     // all white pieces
    nPawns = 2,
    nKnights = 3,
    nBishops = 4,
    nRooks = 5,
    nQueens = 6,
    nKings = 7,
    nEmpty = 8    
};

enum moveType {
    QUIET_MOVES = 0,
    DOUBLE_PAWN_PUSH = 1,
    KING_CASTLE = 2,
    QUEEN_CASTLE = 3,
    CAPTURE = 4,
    EP_CAPTURE = 5,
    KNIGHT_PROMO = 8,
    BISHOP_PROMO = 9,
    ROOK_PROMO = 10,
    QUEEN_PROMO = 11,
    KNIGHT_PROMO_CAPTURE = 12,
    BISHOP_PROMO_CAPTURE = 13,
    ROOK_PROMO_CAPTURE = 14,
    QUEEN_PROMO_CAPTURE = 15
};

// Move masks
constexpr U32 FLAGS_MASK = 0xF000; // bits 12-15
constexpr U8 FLAGS_SHIFT = 12; // shift for flags

constexpr U32 FROM_MASK = 0xFC0; // bits 6-11
constexpr U8 FROM_SHIFT = 6; // shift for from square

constexpr U32 TO_MASK = 0x3F; // bits 0-5
constexpr U8 TO_SHIFT = 0; // shift for to square

constexpr U32 CAPTURED_PIECE_MASK = 0xF0000; // bits 16-19
constexpr U8 CAPTURED_PIECE_SHIFT = 16; // shift for captured piece

// constexpr U32 PREV_FLAGS_MASK = 0xF0000000; // bits 16-19
// constexpr U8 PREV_FLAGS_SHIFT = 16; // shift for previous flags

// constexpr U32 PREV_FROM_MASK = 0xFC00000; // bits 20-25
// constexpr U8 PREV_FROM_SHIFT = 20; // shift for previous from square

// constexpr U32 PREV_TO_MASK = 0x3F0000; // bits 26-31
// constexpr U8 PREV_TO_SHIFT = 26; // shift for previous to square


// Move flag bits (bits 12–15)
constexpr U16 FLAG_SPECIAL_0     = (1 << 12); // bit 12
constexpr U16 FLAG_SPECIAL_1     = (1 << 13); // bit 13
constexpr U16 FLAG_CAPTURE       = (1 << 14); // bit 14
constexpr U16 FLAG_PROMOTION     = (1 << 15); // bit 15

// Specific move types (for clarity)
constexpr U16 FLAG_DOUBLE_PAWN_PUSH = 1; // 0001
constexpr U16 FLAG_KING_CASTLE      = 2; // 0010
constexpr U16 FLAG_QUEEN_CASTLE     = 3; // 0011
constexpr U16 FLAG_CAPTURE_MOVE     = 4; // 0100
constexpr U16 FLAG_EP_CAPTURE       = 5; // 0101

constexpr U16 FLAG_PROMO_KNIGHT     = 8; // 1000
constexpr U16 FLAG_PROMO_BISHOP     = 9; // 1001
constexpr U16 FLAG_PROMO_ROOK       = 10; // 1010
constexpr U16 FLAG_PROMO_QUEEN      = 11; // 1011
constexpr U16 FLAG_PROMO_KNIGHT_CAPTURE     = 12; // 1100
constexpr U16 FLAG_PROMO_BISHOP_CAPTURE     = 13; // 1101
constexpr U16 FLAG_PROMO_ROOK_CAPTURE       = 14; // 1110
constexpr U16 FLAG_PROMO_QUEEN_CAPTURE      = 15; // 1111

// code	promotion	capture	special 1	special 0	kind of move
// 0	0	0	0	0	quiet moves
// 1	0	0	0	1	double pawn push
// 2	0	0	1	0	king castle
// 3	0	0	1	1	queen castle
// 4	0	1	0	0	captures
// 5	0	1	0	1	ep-capture

// 8	1	0	0	0	knight-promotion
// 9	1	0	0	1	bishop-promotion
// 10	1	0	1	0	rook-promotion
// 11	1	0	1	1	queen-promotion
// 12	1	1	0	0	knight-promo capture
// 13	1	1	0	1	bishop-promo capture
// 14	1	1	1	0	rook-promo capture
// 15	1	1	1	1	queen-promo capture

// Bit masks for gameInfo
constexpr U16 TURN_MASK      = 0x1;      // bit 0
constexpr U16 WK_CASTLE      = 0x2;      // bit 1 (white-king castle)
constexpr U16 WQ_CASTLE      = 0x4;      // bit 2 (white-queen castle)
constexpr U16 BK_CASTLE      = 0x8;      // bit 3 (black-king castle)
constexpr U16 BQ_CASTLE      = 0x10;     // bit 4 (black-queen castle)

constexpr U16 MOVE_MASK      = 0x7E0;    // bits 5-10 (6 bits for move count)
constexpr U8 MOVE_SHIFT     = 5;

constexpr U16 EP_IS_SET   = (1 << 11);      // bit 11
constexpr U16 EP_FILE_MASK = (0x7 << 12);   // bits 12-14 (3 bits for file)
constexpr U8 EP_FILE_SHIFT = 12;

// White king-side castle (e1 to g1): squares f1 (5), g1 (6)
constexpr U64 WK_CASTLE_MASK  = (1ULL << 5) | (1ULL << 6);
// White queen-side castle (e1 to c1): squares d1 (3), c1 (2), b1 (1)
constexpr U64 WQ_CASTLE_MASK = (1ULL << 3) | (1ULL << 2) | (1ULL << 1);

// Black king-side castle (e8 to g8): squares f8 (61), g8 (62)
constexpr U64 BK_CASTLE_MASK  = (1ULL << 61) | (1ULL << 62);
// Black queen-side castle (e8 to c8): squares d8 (59), c8 (58), b8 (57)
constexpr U64 BQ_CASTLE_MASK = (1ULL << 59) | (1ULL << 58) | (1ULL << 57);




//  ====== EVALUATION CONSTANTS =====
// these should be moved soon


// white pawn table 
const int wpTable[64] = {
    0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 50, 50, 50, 50, 50,
    10, 10, 20, 30, 30, 20, 10, 10,
    5,  5, 10, 25, 25, 10,  5,  5,
    0,  0,  0, 20, 20,  0,  0,  0,
    5, -5,-10,  0,  0,-10, -5,  5,
    5, 10, 10,-20,-20, 10, 10,  5,
    0,  0,  0,  0,  0,  0,  0,  0
};

const int bpTable[64] = {
    0,  0,  0,  0,  0,  0,  0,  0,
    5, 10, 10,-20,-20, 10, 10,  5,
    5, -5,-10,  0,  0,-10, -5,  5,
    0,  0,  0, 20, 20,  0,  0,  0,
    5,  5, 10, 25, 25, 10,  5,  5,
    10, 10, 20, 30, 30, 20, 10, 10,
    50, 50, 50, 50, 50, 50, 50, 50,
    0,  0,  0,  0,  0,  0,  0,  0
};

// white knight table
const int wnTable[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50
};

// black knight table (mirror of white)
const int bnTable[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50
};

// white bishop table
const int wbTable[64] = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -20,-10,-10,-10,-10,-10,-10,-20,
};

// black bishop table
const int bbTable[64] = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -20,-10,-10,-10,-10,-10,-10,-20,
};

// white rook table
const int wrTable[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
     5, 10, 10, 10, 10, 10, 10,  5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
     0,  0,  0,  5,  5,  0,  0,  0
};

// black rook table
const int brTable[64] = {
     0,  0,  0,  5,  5,  0,  0,  0,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
     0,  0,  0,  0,  0,  0,  0,  0,
     5, 10, 10, 10, 10, 10, 10,  5
};

// white queen table
const int wqTable[64] = {
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5,  5,  5,  5,  0,-10,
     -5,  0,  5,  5,  5,  5,  0, -5,
      0,  0,  5,  5,  5,  5,  0, -5,
    -10,  5,  5,  5,  5,  5,  0,-10,
    -10,  0,  5,  0,  0,  0,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20
};

// black queen table
const int bqTable[64] = {
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  5,  0,  0,  0,  0,-10,
    -10,  5,  5,  5,  5,  5,  0,-10,
      0,  0,  5,  5,  5,  5,  0, -5,
     -5,  0,  5,  5,  5,  5,  0, -5,
    -10,  0,  5,  5,  5,  5,  0,-10,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20
};

// white king middle game table
const int wkMidTable[64] = {
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -10,-20,-20,-20,-20,-20,-20,-10,
     20, 20,  0,  0,  0,  0, 20, 20,
     20, 30, 10,  0,  0, 10, 30, 20
}; 
// black king middle game table
const int bkMidTable[64] = {
     20, 30, 10,  0,  0, 10, 30, 20,
     20, 20,  0,  0,  0,  0, 20, 20,
    -10,-20,-20,-20,-20,-20,-20,-10,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
}; 

// white king end game table
const int wkEndTable[64] = {
    -50,-40,-30,-20,-20,-30,-40,-50,
    -30,-20,-10,  0,  0,-10,-20,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-30,  0,  0,  0,  0,-30,-30,
    -50,-30,-30,-30,-30,-30,-30,-50
};

// black king end game table
const int bkEndTable[64] = {
    -50,-40,-30,-20,-20,-30,-40,-50,
    -30,-30,  0,  0,  0,  0,-30,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-20,-10,  0,  0,-10,-20,-30,
    -50,-40,-30,-20,-20,-30,-40,-50
};




#endif // TYPES_H