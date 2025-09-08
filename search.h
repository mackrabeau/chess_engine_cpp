#ifndef SEARCH_H
#define SEARCH_H

#include "evaluation.h"
#include "transposition.h"
#include <unordered_map>

const int MATE_VALUE = 30000;
const int MATE_THRESHOLD = 29000;  
const int STALEMATE_VALUE = 0;

extern long g_nodeCount;
extern long g_ttHits;
extern long g_ttProbes;

extern std::chrono::steady_clock::time_point g_searchStartTime;
extern long g_timeLimit;

const int MAX_SEARCH_DEPTH = 50;
extern Move killerMoves[MAX_SEARCH_DEPTH][2];

// record search tree for debugging
extern std::vector<std::string> g_searchTree;
extern int g_currentPly;
extern bool g_recordSearchTree;
extern size_t g_searchTreeMaxLines;

using namespace evaluation;

void startSearchTree();
void stopAndPrintSearchTree(size_t maxLines = 100000);
void recordEntry(const Game& game, int depth, int alpha, int beta);
void recordExit(const Game& game, int depth, int score);

bool isTimeUp();

Move searchAtDepth(Game& game, int depth);
int quiescenceSearch(int alpha, int beta, Game& game, int qDepth);
int alphabeta(int alpha, int beta, int depth, Game& game);
int getTerminalValue(Game& game, int depth);
void resetSearchStats();
void printSearchStats();

int adjustMateScore(int score, int ply);
int restoreMateScore(int score, int ply);
void updateKillerMove(Move move, int depth);
bool isKillerMove(Move move, int depth);

static inline int evalForSide(const Game& game) {
    int whiteScore = evaluateBoard(game.board); // always white-perspective
    return (game.board.gameInfo & 1) ? whiteScore : -whiteScore;
}



#endif // SEARCH_H