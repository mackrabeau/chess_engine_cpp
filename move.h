#ifndef MOVE_H
#define MOVE_H

#include "types.h"

#include <cstdint>
#include <iostream>

typedef uint16_t U16;
typedef uint32_t U32;

using namespace std;

class Move {
public:

    inline Move() : move(0) {}
    inline Move(U32 m) : move(m) {}

    inline Move(U8 from, U8 to, int epSquare, enumPiece piece, enumPiece target, enumPiece promoType = nEmpty){

        move = 0;
        setFrom(from);
        setTo(to);

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

        // promotion piece selection (for now, always queen)
        // int promotionType = 0; // 0=knight, 1=bishop, 2=rook, 3=queen
        // if (isPromotion) {
        //     promotionType = 3; // assume user chose queen
        // }
        
        if (promoType != nEmpty) {
            if (isCapture) {
                switch (promoType) {
                    case nKnights: setFlags(KNIGHT_PROMO_CAPTURE); break;
                    case nBishops: setFlags(BISHOP_PROMO_CAPTURE); break;
                    case nRooks:   setFlags(ROOK_PROMO_CAPTURE);   break;
                    case nQueens:  setFlags(QUEEN_PROMO_CAPTURE);  break;
                    case nBlack:
                    case nWhite:
                    case nPawns:
                    case nKings:
                    case nEmpty: // do nothing for these cases
                        break;
                }
            } else {
                switch (promoType) {
                    case nKnights: setFlags(KNIGHT_PROMO); break;
                    case nBishops: setFlags(BISHOP_PROMO); break;
                    case nRooks:   setFlags(ROOK_PROMO);   break;
                    case nQueens:  setFlags(QUEEN_PROMO);  break;
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
                setFlags(EP_CAPTURE);
            else
                setFlags(CAPTURE);
        } else if (isDoublePawnPush) {
            setFlags(DOUBLE_PAWN_PUSH);
        } else if (isCastle) {
            if (isQueenCastle)
                setFlags(QUEEN_CASTLE);
            else
                setFlags(KING_CASTLE);
        } else {
            setFlags(QUIET_MOVES); // quiet move
        }

        setCapturedPiece(target);
    }
    
    inline U32 getMove() const { return move; }

    inline int getFrom() const { return (move & FROM_MASK) >> FROM_SHIFT; } // extract from square
    inline int getTo() const { return (move & TO_MASK) >> TO_SHIFT; }
    inline int getFlags() const { return (move & FLAGS_MASK) >> FLAGS_SHIFT; } // extract flags
    inline moveType getMoveType() const { return static_cast<moveType>(getFlags()); }

    inline void setFrom(int from) { 
        move = (move & ~FROM_MASK) | (from << FROM_SHIFT); 
    } // set from square
    inline void setTo(int to) { 
        move = (move & ~TO_MASK) | (to << TO_SHIFT); 
    }
    inline void setFlags(int flags) { 
        move = (move & ~FLAGS_MASK) | (flags << FLAGS_SHIFT); 
    } // set flags

    inline void setCapturedPiece(enumPiece piece) {
        move = (move & ~CAPTURED_PIECE_MASK) | (static_cast<U32>(piece) << CAPTURED_PIECE_SHIFT);
    }

    inline enumPiece getCapturedPiece() const {
        return static_cast<enumPiece>((move & CAPTURED_PIECE_MASK) >> CAPTURED_PIECE_SHIFT);
    }

    // inline void setPrevFrom(int from) { move = (move & PREV_FROM_MASK) | (from << PREV_FROM_SHIFT); } // set from square
    // inline void setPrevTo(int to) { move = (move & PREV_TO_MASK) | (to << PREV_TO_SHIFT); }
    // inline void setPrevFlags(int flags) { move = (move & PREV_FLAGS_MASK) | (flags << PREV_FLAGS_SHIFT); } // set flags

    inline bool isQuiet() const { return ((move & FLAGS_MASK) >> FLAGS_SHIFT) == QUIET_MOVES; } // quiet move
    inline bool isDoublePawnPush() const { return ((move & FLAGS_MASK) >> FLAGS_SHIFT) == DOUBLE_PAWN_PUSH; }
    inline bool isKingCastle() const { return ((move & FLAGS_MASK) >> FLAGS_SHIFT) == KING_CASTLE; }
    inline bool isQueenCastle() const { return ((move & FLAGS_MASK) >> FLAGS_SHIFT) == QUEEN_CASTLE; }
    inline bool isEPCapture() const { return ((move & FLAGS_MASK) >> FLAGS_SHIFT) == EP_CAPTURE; }
    inline bool isPromotion() const { return ((move & FLAGS_MASK) >> FLAGS_SHIFT) >= KNIGHT_PROMO && ((move & FLAGS_MASK) >> FLAGS_SHIFT) <= QUEEN_PROMO; } // knight, bishop, rook, queen promotion


    

    inline bool isPromoCapture() const {
        int flags = (move & FLAGS_MASK) >> FLAGS_SHIFT;
        return (flags >= KNIGHT_PROMO_CAPTURE && flags <= QUEEN_PROMO_CAPTURE);
    }

    inline bool isCapture() const { 
        int flags = (move & FLAGS_MASK) >> FLAGS_SHIFT;
        return (flags == CAPTURE || 
                flags == EP_CAPTURE || 
                (flags >= KNIGHT_PROMO_CAPTURE && flags <= QUEEN_PROMO_CAPTURE));
    }

    inline int getPromotionType() const { return (move >> (FLAGS_SHIFT + 1)) & 0x3; }
    
    inline enumPiece getPromotionPiece() const {
        switch (getFlags()) {
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


    inline void setPromotionType(int type){ 
        move &= ~(0x3 << (FLAGS_SHIFT + 1)); 
        move |= (type << (FLAGS_SHIFT + 1));  
    }


    void display() const {
        std::cout << "Move: " << move << ", From: " << getFrom() << ", To: " << getTo() << ", Flags: " << getFlags() << "\n";
    }

    std::string toString() const;

private:
    U32 move;

};

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
            std::cout << "Move list is full, cannot add more moves. fix later\n";
        }
    }

    void clear() {
        count = 0;
    }

    void displayMoves() const {
        for (int i = 0; i < count; i++) {
            displayMove(i);
        }
    }

    void displayMove(int i) const {
        if (i < 0 || i >= count) return; // invalid index

        moveList[i].display();
    }

    int getNumMoves() const { return count; } // returns number of legal moves 
    Move getMove(int i) const { return moveList[i]; }

};


#endif // MOVE_H
