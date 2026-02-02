#ifndef EVALUATION_H
#define EVALUATION_H

#include "board.h"
#include "game.h"
#include "types.h"

#include <vector>

// Forward declaration
namespace nnue {
    class NNUE;
    class Accumulator;
}

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

// Evaluation mode
enum class EvalMode {
    TRADITIONAL,  // Use traditional piece-square table evaluation
    NNUE,         // Use NNUE neural network evaluation
    HYBRID        // Use both and combine (for testing)
};

// Global evaluation mode setting
extern EvalMode g_evalMode;

// Global NNUE network instance (nullptr if not initialized)
extern nnue::NNUE* g_nnueNetwork;

// Set evaluation mode
void setEvalMode(EvalMode mode);

// Initialize NNUE network (loads model if path provided, otherwise uses random weights)
bool initializeNNUE(const std::string& modelPath = "");

// Cleanup NNUE network
void cleanupNNUE();

int evaluateBoard(const Board& board);
int evaluateBoardTraditional(const Board& board);
int evaluateBoardNNUE(const Board& board, nnue::Accumulator* accumulator = nullptr);
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