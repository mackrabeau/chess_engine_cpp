#ifndef SEARCH_H
#define SEARCH_H

#include "evaluation.h"
#include "transposition.h"
#include "move.h"
#include <vector>
#include <atomic>
#include <chrono>

const int MATE_VALUE = 30000;
const int MATE_THRESHOLD = 29000;
const int STALEMATE_VALUE = 0;
const int MAX_SEARCH_DEPTH = 50;

using namespace evaluation;

// All state local to a single search lives here instead of in globals, so
// independent searches (e.g. self-play, tests, UCI) don't cross-talk. The
// transposition table (g_transpositionTable) stays a separate global since it
// is meant to eventually be shared across concurrent searches.
struct SearchContext {
    // stats, reset at the start of each search via reset()
    long nodeCount = 0;
    long ttHits = 0;
    long ttProbes = 0;

    // limits, set by the caller before starting a search
    long nodeLimit = -1;      // -1 = unlimited
    long timeLimitMs = 20000; // ms

    // move-ordering memory, indexed by depth
    Move killerMoves[MAX_SEARCH_DEPTH][2] = {};

    // ply-from-root, tracked by alphabeta() entry/exit; used to convert
    // between mate scores stored in the TT and mate distance from the root.
    int currentPly = 0;

    // leaf evaluator; defaults to the classical evaluator, swappable later
    // for a learned one.
    const evaluation::Evaluator* evaluator = &evaluation::defaultEvaluator();

    std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
    bool timeoutOccurred = false;
    std::atomic<bool> stopRequested{false};

    // Resets stats/killers/ply/timeout for a fresh search. Leaves nodeLimit,
    // timeLimitMs, and evaluator alone -- the caller configures those first.
    void reset() {
        nodeCount = 0;
        ttHits = 0;
        ttProbes = 0;
        currentPly = 0;
        timeoutOccurred = false;
        startTime = std::chrono::steady_clock::now();
        stopRequested.store(false, std::memory_order_relaxed);
        for (int depth = 0; depth < MAX_SEARCH_DEPTH; ++depth) {
            killerMoves[depth][0] = MOVE_NONE;
            killerMoves[depth][1] = MOVE_NONE;
        }
    }

    void requestStop() { stopRequested.store(true, std::memory_order_relaxed); }
    void clearStopRequest() { stopRequested.store(false, std::memory_order_relaxed); }
    bool stopWasRequested() const { return stopRequested.load(std::memory_order_relaxed); }

    void updateKillerMove(Move move, int depth) {
        if (depth < 0 || depth >= MAX_SEARCH_DEPTH) return;
        if (killerMoves[depth][0] != move) {
            killerMoves[depth][1] = killerMoves[depth][0];
            killerMoves[depth][0] = move;
        }
    }

    bool isKillerMove(Move move, int depth) const {
        if (depth < 0 || depth >= MAX_SEARCH_DEPTH) return false;
        return killerMoves[depth][0] == move || killerMoves[depth][1] == move;
    }

    // checked every node; true once the search should stop
    bool timeUp() {
        if (stopWasRequested()) return true;
        if (nodeLimit > 0 && nodeCount >= nodeLimit) return true;
        if (timeoutOccurred) return true;

        // check wall clock every 1024 nodes for efficiency
        if (nodeCount % 1024 == 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTime).count();
            if (elapsed > timeLimitMs) {
                timeoutOccurred = true;
                return true;
            }
        }
        return false;
    }
};

Move searchAtDepth(Game& game, int depth, SearchContext& ctx, const std::vector<Move>* rootFilter = nullptr);
int quiescenceSearch(int alpha, int beta, Game& game, int qDepth, SearchContext& ctx);
int alphabeta(int alpha, int beta, int depth, Game& game, SearchContext& ctx);
int getTerminalValue(Game& game, const SearchContext& ctx);
void printSearchStats(const SearchContext& ctx);

int adjustMateScore(int score, int ply);
int restoreMateScore(int score, int ply);

inline int evalForSide(const Game& game, const SearchContext& ctx) {
    int whiteScore = ctx.evaluator->evaluate(game.board); // always white-perspective
    return (game.board.gameInfo & 1) ? whiteScore : -whiteScore;
}

#endif // SEARCH_H