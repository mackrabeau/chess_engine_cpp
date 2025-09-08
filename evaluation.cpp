#include "evaluation.h"
#include <array>
#include <cmath>

namespace evaluation {

const int INIT_TOTAL = Pwt * 16 + Nwt * 4 + Bwt * 4 + Rwt * 4 + Qwt * 2;

int evaluateBoard(const Board& board) {

    U64 pieces = board.getAllPieces();
    
    int score = 0;

    while (pieces) {
        int square = __builtin_ctzll(pieces);  // get the least significant set bit
        pieces &= pieces - 1;                  // remove the least significant set bit

        enumPiece pieceType = board.getPieceType(square);
        enumPiece pieceColour = board.getColourType(square);

        switch (pieceType) {
            case nPawns:
                pieceColour == nWhite ? score += wpTable[square] : score -= bpTable[square]; // Add pawn evaluation logic
                break;
            case nKnights:
                pieceColour == nWhite ? score += wnTable[square] : score -= bnTable[square]; // Add knight evaluation logic
                break;
            case nBishops:
                pieceColour == nWhite ? score += wbTable[square] : score -= bbTable[square]; // Add bishop evaluation logic
                break;
            case nRooks:
                pieceColour == nWhite ? score += wrTable[square] : score -= brTable[square]; // Add rook evaluation logic
                break;
            case nQueens:
                pieceColour == nWhite ? score += wqTable[square] : score -= bqTable[square]; // Add queen evaluation logic
                break;
            case nKings:
                pieceColour == nWhite ? score += wkMidTable[square] : score -= bkMidTable[square]; // Add king evaluation logic
                break;
            default:
                break;
        }
    }

    return score + materialScore(board);

    // int v = score + materialScore(board);
    // return (board.gameInfo & 1) ? v : -v; // if black to move, invert scores
}

int materialScore(const Board& board) {
    
    int score = 0;

    score += __builtin_popcountll(board.getWhitePawns()) * Pwt;   // Pawns
    score += __builtin_popcountll(board.getWhiteKnights()) * Nwt; // Knights
    score += __builtin_popcountll(board.getWhiteBishops()) * Bwt; // Bishops
    score += __builtin_popcountll(board.getWhiteRooks()) * Rwt;   // Rooks
    score += __builtin_popcountll(board.getWhiteQueens()) * Qwt;  // Queens

    score -= __builtin_popcountll(board.getBlackPawns()) * Pwt;
    score -= __builtin_popcountll(board.getBlackKnights()) * Nwt;
    score -= __builtin_popcountll(board.getBlackBishops()) * Bwt;
    score -= __builtin_popcountll(board.getBlackRooks()) * Rwt;
    score -= __builtin_popcountll(board.getBlackQueens()) * Qwt;

    return score;
}


std::vector<float> piecePlanes(const Board& board) {
    std::vector<float> output;
    output.reserve(12 * 64); // 12 planes (6 pieces, 2 colours) for each of the 64 squares

    auto push_plane = [&](U64 pieceBB) {
        for (int sq = 0; sq < 64; ++sq) {
            output.push_back((pieceBB & (1ULL << sq)) ? 1.0f : 0.0f);
        }
    };

    // white planes
    push_plane(board.getWhitePawns());
    push_plane(board.getWhiteKnights());
    push_plane(board.getWhiteBishops());
    push_plane(board.getWhiteRooks());
    push_plane(board.getWhiteQueens());
    push_plane(board.getWhiteKing());

    // black planes
    push_plane(board.getBlackPawns());
    push_plane(board.getBlackKnights());
    push_plane(board.getBlackBishops());
    push_plane(board.getBlackRooks());
    push_plane(board.getBlackQueens());
    push_plane(board.getBlackKing());

    return output;
}

// heuristic to compute what phase the game is in, normalized [0.. 1]
// 1 is opening, 0 is endgame
float computePhase(const Board& board) {

    int white_sum =
        __builtin_popcountll(board.getWhitePawns()) * Pwt +
        __builtin_popcountll(board.getWhiteKnights()) * Nwt +
        __builtin_popcountll(board.getWhiteBishops()) * Bwt +
        __builtin_popcountll(board.getWhiteRooks()) * Rwt +
        __builtin_popcountll(board.getWhiteQueens()) * Qwt;

    int black_sum =
        __builtin_popcountll(board.getBlackPawns()) * Pwt +
        __builtin_popcountll(board.getBlackKnights()) * Nwt +
        __builtin_popcountll(board.getBlackBishops()) * Bwt +
        __builtin_popcountll(board.getBlackRooks()) * Rwt +
        __builtin_popcountll(board.getBlackQueens()) * Qwt;

    int total = white_sum + black_sum;
    float phase = (float)total / (float)INIT_TOTAL;
    if (phase < 0.0f) phase = 0.0f;
    if (phase > 1.0f) phase = 1.0f;
    return phase;
}




}
