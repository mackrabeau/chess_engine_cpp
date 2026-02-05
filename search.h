#ifndef SEARCH_H
#define SEARCH_H

#include "evaluation.h"
#include "transposition.h"
#include "nnue.h"
#include <unordered_map>
#include <vector>
#include <atomic>

const int MATE_VALUE = 30000;
const int MATE_THRESHOLD = 29000;  
const int STALEMATE_VALUE = 0;

extern long g_nodeCount;
extern long g_ttHits;
extern long g_ttProbes;

extern std::chrono::steady_clock::time_point g_searchStartTime;
extern long g_timeLimit;
extern long g_nodeLimit;

const int MAX_SEARCH_DEPTH = 50;
extern Move killerMoves[MAX_SEARCH_DEPTH][2];

// record search tree for debugging
extern std::vector<std::string> g_searchTree;
extern int g_currentPly;
extern bool g_recordSearchTree;
extern size_t g_searchTreeMaxLines;

struct SearchResult {
    Move bestMove;
    int score;
    bool found;

    SearchResult() : bestMove(MOVE_NONE), score(0), found(false) {}
};


using namespace evaluation;

void startSearchTree();
void stopAndPrintSearchTree(size_t maxLines = 100000);
void recordEntry(const Game& game, int depth, int alpha, int beta);
void recordExit(const Game& game, int depth, int score);

bool isTimeUp();
void requestStopSearch();
void resetStopSearchFlag();
bool isStopSearchRequested();
void setNodeLimit(long limit);

SearchResult searchAtDepthWithScore(Game& game, int depth, const std::vector<Move>* rootFilter = nullptr);
Move searchAtDepth(Game& game, int depth, const std::vector<Move>* rootFilter = nullptr);
int quiescenceSearch(int alpha, int beta, Game& game, int qDepth);
int alphabeta(int alpha, int beta, int depth, Game& game);
int getTerminalValue(Game& game);
void resetSearchStats();
void printSearchStats();

int adjustMateScore(int score, int ply);
int restoreMateScore(int score, int ply);
void updateKillerMove(Move move, int depth);
bool isKillerMove(Move move, int depth);

static inline int evalForSide(const Game& game) {
    using namespace evaluation;
    // Use accumulator if available and NNUE is enabled
    if (g_evalMode != EvalMode::TRADITIONAL && game.nnueAccumulator != nullptr && g_nnueNetwork != nullptr) {
        float nnueEval = g_nnueNetwork->evaluate(*game.nnueAccumulator);
        // Network outputs in pawns, so multiply by 100 to get centipawns
        int whiteScore = static_cast<int>(nnueEval * 100.0f);
        return (game.board.gameInfo & 1) ? whiteScore : -whiteScore;
    }
    
    // Fallback to traditional evaluation
    int whiteScore = evaluateBoard(game.board); // always white-perspective
    return (game.board.gameInfo & 1) ? whiteScore : -whiteScore;
}



#endif // SEARCH_H