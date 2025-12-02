#ifndef MOVE_H
#define MOVE_H

#include "types.h"

#include <cstdint>
#include <iostream>

typedef uint16_t U16;
typedef uint32_t U32;

typedef U32 Move;


inline int getFrom(Move m) { return (m & FROM_MASK) >> FROM_SHIFT; } 
inline int getTo(Move m) { return (m & TO_MASK) >> TO_SHIFT; }
inline int getFlags(Move m) { return (m & FLAGS_MASK) >> FLAGS_SHIFT; }
inline moveType getMoveType(Move m) { return static_cast<moveType>(getFlags(m)); }

// set flags
inline Move setFrom(Move m, int from) { return (m & ~FROM_MASK) | (from << FROM_SHIFT); }
inline Move setTo(Move m, int to) { return (m & ~TO_MASK) | (to << TO_SHIFT); }
inline Move setFlags(Move m, int flags) { return (m & ~FLAGS_MASK) | (flags << FLAGS_SHIFT); }

inline Move setCapturedPiece(Move m, enumPiece piece) {
    return (m & ~CAPTURED_PIECE_MASK) | (static_cast<U32>(piece) << CAPTURED_PIECE_SHIFT);
}

inline enumPiece getCapturedPiece(Move m) {
    return static_cast<enumPiece>((m & CAPTURED_PIECE_MASK) >> CAPTURED_PIECE_SHIFT);
}

inline bool isQuiet(Move m) { return ((m & FLAGS_MASK) >> FLAGS_SHIFT) == QUIET_MOVES; } // quiet move
inline bool isDoublePawnPush(Move m) { return ((m & FLAGS_MASK) >> FLAGS_SHIFT) == DOUBLE_PAWN_PUSH; }
inline bool isKingCastle(Move m) { return ((m & FLAGS_MASK) >> FLAGS_SHIFT) == KING_CASTLE; }
inline bool isQueenCastle(Move m) { return ((m & FLAGS_MASK) >> FLAGS_SHIFT) == QUEEN_CASTLE; }
inline bool isCastle(Move m) {return isQueenCastle(m) || isKingCastle(m); }
inline bool isEPCapture(Move m) { return ((m & FLAGS_MASK) >> FLAGS_SHIFT) == EP_CAPTURE; }
inline bool isPromotion(Move m) { return ((m & FLAGS_MASK) >> FLAGS_SHIFT) >= KNIGHT_PROMO && ((m & FLAGS_MASK) >> FLAGS_SHIFT) <= QUEEN_PROMO; } // knight, bishop, rook, queen promotion

inline bool isPromoCapture(Move m) {
    int flags = (m & FLAGS_MASK) >> FLAGS_SHIFT;
    return (flags >= KNIGHT_PROMO_CAPTURE && flags <= QUEEN_PROMO_CAPTURE);
}

inline bool isCapture(Move m) { 
    int flags = (m & FLAGS_MASK) >> FLAGS_SHIFT;
    return (flags == CAPTURE || 
            flags == EP_CAPTURE || 
            (flags >= KNIGHT_PROMO_CAPTURE && flags <= QUEEN_PROMO_CAPTURE));
}

inline int getPromotionType(Move m) { return (m >> (FLAGS_SHIFT + 1)) & 0x3; }

inline enumPiece getPromotionPiece(Move m) {
    switch (getFlags(m)) {
        case KNIGHT_PROMO:
        case KNIGHT_PROMO_CAPTURE:
            return nKnights;
        case BISHOP_PROMO:
        case BISHOP_PROMO_CAPTURE:
            return nBishops;
        case ROOK_PROMO:
        case ROOK_PROMO_CAPTURE:    
            return nRooks;
        case QUEEN_PROMO:
        case QUEEN_PROMO_CAPTURE:
            return nQueens;
        default:
            return nEmpty; // should not happen
    }
}

inline Move setPromotionType(Move m, int type){ 
    m &= ~(0x3 << (FLAGS_SHIFT + 1)); 
    m |= (type << (FLAGS_SHIFT + 1));  
    return m;
}

inline Move makeMove(U8 from, U8 to, int epSquare, enumPiece piece, enumPiece target, enumPiece promoType = nEmpty){
    Move m = 0;
    m = setFrom(m, from);
    m = setTo(m, to);

    // special moves 
    bool isCapture = (target != nEmpty);

    bool isEnPassant = false;
    if (piece == nPawns) {
        if (epSquare != -1 && to == epSquare && target == nEmpty) {
            isCapture = true; // en passant capture
            isEnPassant = true;
        }
    } 
    bool isDoublePawnPush = (piece == nPawns) && (abs((int)from - (int)to) == 16);
    bool isCastle = (piece == nKings) && (abs((int)from - (int)to) == 2);
    bool isQueenCastle = isCastle && (to == 2 || to == 58);
    
    if (promoType != nEmpty) {
        if (isCapture) {
            switch (promoType) {
                case nKnights: m = setFlags(m, KNIGHT_PROMO_CAPTURE); break;
                case nBishops: m = setFlags(m, BISHOP_PROMO_CAPTURE); break;
                case nRooks:   m = setFlags(m, ROOK_PROMO_CAPTURE);   break;
                case nQueens:  m = setFlags(m, QUEEN_PROMO_CAPTURE);  break;
                case nBlack:
                case nWhite:
                case nPawns:
                case nKings:
                case nEmpty: // do nothing for these cases
                    break;
            }
        } else {
            switch (promoType) {
                case nKnights: m = setFlags(m, KNIGHT_PROMO); break;
                case nBishops: m = setFlags(m, BISHOP_PROMO); break;
                case nRooks:   m = setFlags(m, ROOK_PROMO);   break;
                case nQueens:  m = setFlags(m, QUEEN_PROMO);  break;
                case nBlack:
                case nWhite:
                case nPawns:
                case nKings:
                case nEmpty: // do nothing for these cases
                    break;
            }
        }
    } else if (isCapture) {
        if (isEnPassant)
            m = setFlags(m, EP_CAPTURE);
        else
            m = setFlags(m, CAPTURE);
    } else if (isDoublePawnPush) {
        m = setFlags(m, DOUBLE_PAWN_PUSH);
    } else if (isCastle) {
        if (isQueenCastle)
            m = setFlags(m, QUEEN_CASTLE);
        else
            m = setFlags(m, KING_CASTLE);
    } else {
        m = setFlags(m, QUIET_MOVES); // quiet move
    }

    m = setCapturedPiece(m, target);
    return m;
}

constexpr Move MOVE_NONE = 0;

// inline void displayMove(Move m) {
//     std::cout << "Move: " << m << ", From: " << getFrom(m) << ", To: " << getTo(m) << ", Flags: " << getFlags(m) << "\n";
// }
std::string moveToString(Move m);

static const int MAX_MOVES = 218; // max number of moves

struct MovesStruct {
    Move moveList[MAX_MOVES]; // max number of moves
    short count; // counts number of moves

    MovesStruct() : count(0) {}

    void addMove(Move move){
        if (count < MAX_MOVES) {
            moveList[count] = move;
            count++;
        } else {
            std::cout << "Move list is full, cannot add more moves. Fix later\n";
        }
    }

    void clear() {
        count = 0;
    }

    void displayMoves() const {
        for (int i = 0; i < count; i++) {
            std::cout << moveToString(moveList[i]) << "\n";
        }
    }

    void displayMove(int i) const {
        if (i < 0 || i >= count) return; // invalid index
        std::cout << moveToString(moveList[i]) << "\n";
    }

    int getNumMoves() const { return count; } // returns number of legal moves 
    Move getMove(int i) const { return moveList[i]; }

};


#endif // MOVE_H
