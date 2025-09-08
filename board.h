#ifndef BOARD_H
#define BOARD_H

#include "types.h"
#include "move.h"

#include <iostream>
#include <cstdint>
#include <string>
#include <vector>

using namespace std;

class Board {
public:
    U64 pieceBB[8];
    U16 gameInfo;
    U64 hash;

    Board(const std::string& fen="rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"); 
    Board(const Board& other);
    Board(U64 otherPieceBB[8], const U16& otherGameInfo, const U64& otherHash);
    std::string toString() const; // FEN representation

    inline int getEnPassantSquare() const {
        if (!(gameInfo & EP_IS_SET)) return -1; // No en passant square

        int epCol = (gameInfo & EP_FILE_MASK) >> EP_FILE_SHIFT; // get en passant column from game info
        int epRow = (friendlyColour() == nWhite) ? 5 : 2; // row 5 for white, 2 for black
        return (epRow * 8 + epCol);
    }

    inline void setPiece(int square, enumPiece piece, enumPiece colour) {
        pieceBB[piece] |= (1ULL << square);
        pieceBB[colour] |= (1ULL << square);
        // hash ^= MoveTables::instance().zobristTable[getPieceIndex(piece, colour)][square];
    }

    inline void removePiece(int square, enumPiece piece, enumPiece colour) {
        pieceBB[piece] &= ~(1ULL << square);
        pieceBB[colour] &= ~(1ULL << square);
        // hash ^= MoveTables::instance().zobristTable[getPieceIndex(piece, colour)][square];
    }

    inline void setEpSquare(int square) {
        int file = square % 8; // get the file of the square
        gameInfo |= EP_IS_SET | ((file << EP_FILE_SHIFT) & EP_FILE_MASK); // set the en passant square
        // hash ^= MoveTables::instance().zobristEnPassant[file]; // add new en passant square to hash
    }

    inline void clearEpSquare() {
        if (!(gameInfo & EP_IS_SET)) return;

        int epFile = (gameInfo & EP_FILE_MASK) >> EP_FILE_SHIFT;
        // hash ^= MoveTables::instance().zobristEnPassant[epFile]; // remove old en passant square from hash
        gameInfo &= ~(EP_IS_SET | EP_FILE_MASK); // clear en passant square
    }

    inline int getCastlingIndex() const {
        int castlingIndex = 0;
        if (gameInfo & WK_CASTLE) castlingIndex |= 1;
        if (gameInfo & WQ_CASTLE) castlingIndex |= 2;
        if (gameInfo & BK_CASTLE) castlingIndex |= 4;
        if (gameInfo & BQ_CASTLE) castlingIndex |= 8;
        return castlingIndex;
        // hash ^= MoveTables::instance().zobristCastling[castlingIndex];
    }

    inline void updateCastlePieces(moveType moveType, enumPiece piece, enumPiece colour){
        // Handle special moves like castling and en passant captures
        if (moveType == KING_CASTLE) {
            if (colour == nWhite) {
                // Move the rook from h1 to f1
                removePiece(7, nRooks, nWhite);
                setPiece(5, nRooks, nWhite);
            } else {
                // Move the rook from h8 to f8
                removePiece(63, nRooks, nBlack);
                setPiece(61, nRooks, nBlack);
            }
        } else if (moveType == QUEEN_CASTLE) {
            if (colour == nWhite) {
                // Move the rook from a1 to d1
                removePiece(0, nRooks, nWhite);
                setPiece(3, nRooks, nWhite);
            } else {
                // Move the rook from a8 to d8
                removePiece(56, nRooks, nBlack);
                setPiece(59, nRooks, nBlack);
            }
        }
    }

    inline void updateCasltingRights(enumPiece piece, enumPiece colour, int from) {
        // update castling rights
        if (piece == nKings) {
            // remove castling rights for the king
            if (colour == nWhite) {
                gameInfo &= ~(WK_CASTLE | WQ_CASTLE);
            } else {
                gameInfo &= ~(BK_CASTLE | BQ_CASTLE);
            }
        } else if (piece == nRooks) {
            // remove castling rights for the rook
            if (colour == nWhite) {
                if (from == 0) gameInfo &= ~WQ_CASTLE; // a1 rook
                if (from == 7) gameInfo &= ~WK_CASTLE; // h1 rook
            } else {
                if (from == 56) gameInfo &= ~BQ_CASTLE; // a8 rook
                if (from == 63) gameInfo &= ~BK_CASTLE; // h8 rook
            }
        }

        // update castling zorbist hash
        int newCastlingIdx = 0;
        if (gameInfo & WK_CASTLE) newCastlingIdx |= 1;
        if (gameInfo & WQ_CASTLE) newCastlingIdx |= 2;
        if (gameInfo & BK_CASTLE) newCastlingIdx |= 4;
        if (gameInfo & BQ_CASTLE) newCastlingIdx |= 8;
        // hash ^= MoveTables::instance().zobristCastling[newCastlingIdx];
    }

    // void updateGameInfo(moveType moveType, enumPiece piece);

    inline U64 getAllPieces() const {return pieceBB[nWhite] | pieceBB[nBlack];};
    inline U64 getWhitePieces() const {return pieceBB[nWhite];};
    inline U64 getBlackPieces() const {return pieceBB[nBlack];};

    inline U64 getFriendlyPieces() const {return pieceBB[friendlyColour()];};
    inline U64 getEnemyPieces() const {return pieceBB[enemyColour()];};

    inline U64 getWhitePawns() const {return pieceBB[nPawns] & pieceBB[nWhite];}
    inline U64 getWhiteKnights() const {return pieceBB[nKnights] & pieceBB[nWhite];}
    inline U64 getWhiteBishops() const {return pieceBB[nBishops] & pieceBB[nWhite];}
    inline U64 getWhiteRooks() const {return pieceBB[nRooks] & pieceBB[nWhite];}
    inline U64 getWhiteQueens() const {return pieceBB[nQueens] & pieceBB[nWhite];}
    inline U64 getWhiteKing() const {return pieceBB[nKings] & pieceBB[nWhite];}

    inline U64 getBlackPawns() const {return pieceBB[nPawns] & pieceBB[nBlack];}
    inline U64 getBlackKnights() const {return pieceBB[nKnights] & pieceBB[nBlack];}
    inline U64 getBlackBishops() const {return pieceBB[nBishops] & pieceBB[nBlack];}
    inline U64 getBlackRooks() const {return pieceBB[nRooks] & pieceBB[nBlack];}
    inline U64 getBlackQueens() const {return pieceBB[nQueens] & pieceBB[nBlack];}
    inline U64 getBlackKing() const {return pieceBB[nKings] & pieceBB[nBlack];}

    inline int getHalfMoveClock() const {
        return (gameInfo & MOVE_MASK) >> MOVE_SHIFT; // extract the halfmove clock from game info
    }

    inline enumPiece getPieceType(int square) const {
        if (pieceBB[nPawns] >> square & 1) return nPawns;
        if (pieceBB[nBishops] >> square & 1) return nBishops;
        if (pieceBB[nKnights] >> square & 1) return nKnights;
        if (pieceBB[nRooks] >> square & 1) return nRooks;
        if (pieceBB[nQueens] >> square & 1) return nQueens;
        if (pieceBB[nKings] >> square & 1) return nKings;
        return nEmpty;
    }

    inline enumPiece getColourType(int square) const {
        if (pieceBB[nWhite] >> square & 1){return nWhite;}
        if (pieceBB[nBlack] >> square & 1){return nBlack;}
        // return nEmpty;
        throw std::invalid_argument( "received empty square");
    }
    inline enumPiece friendlyColour() const { return (gameInfo & TURN_MASK) ? nWhite : nBlack; }
    inline enumPiece enemyColour() const { return (gameInfo & TURN_MASK) ? nBlack : nWhite; }

    inline int getPieceIndex(enumPiece piece, enumPiece color) const { // get the zobrist index for a piece at a square
        // Map to 0-11 range
        switch(piece) {
            case nPawns:   return (color == nWhite) ? 0 : 6;
            case nKnights: return (color == nWhite) ? 1 : 7;
            case nBishops: return (color == nWhite) ? 2 : 8;
            case nRooks:   return (color == nWhite) ? 3 : 9;
            case nQueens:  return (color == nWhite) ? 4 : 10;
            case nKings:   return (color == nWhite) ? 5 : 11;
            default: return -1;
        }
    }

    inline int getPieceIndex(int square) const {
        enumPiece piece = getPieceType(square);
        enumPiece color = getColourType(square);
        return getPieceIndex(piece, color);
    }

    inline U64 getHash() const { return this->hash; }

    void calculateHash(); // calculate the hash for the current board state

    void displayBoard() const;
    void displayGameInfo() const; 
    void coloursTurnToString() const;
    char pieceToChar(int square) const;

private:

    void clearBoard();
    void loadPiece(char piece, int square);

    int colourCode(enumPiece ct) const;
    int pieceCode(enumPiece pt) const;
};

#endif // BOARD_H
