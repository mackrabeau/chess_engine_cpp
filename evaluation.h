#ifndef EVALUATION_H
#define EVALUATION_H

#include "board.h"
#include "game.h"
#include "types.h"

#include <vector>

// values taken from https://www.chessprogramming.org/Simplified_Evaluation_Function
// ^^ FOR NOW ^^

namespace evaluation {


// piece values in centipawns
// just placeholder values, learn these weights through the model
const int Pwt = 100;
const int Nwt = 320;
const int Bwt = 330;
const int Rwt = 500;
const int Qwt = 900;
// const int Kwt = 20000;

int evaluateBoard(const Board& board);
int materialScore(const Board& board);

// plane / feature extractors
// std::vector<float> piecePlanes(const Board& board);


inline int pieceScore(const enumPiece& pieceType){
    switch (pieceType) {
        case nPawns: return Pwt;
        case nKnights: return Nwt;
        case nBishops: return Bwt;
        case nRooks: return Rwt;
        case nQueens: return Qwt;
        // case nKings: return Kwt;
        default: return 0; // Should not happen
    }
}

}


#endif // EVALUATION_H