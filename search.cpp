#include "search.h"
#include "transposition.h"
#include <chrono>
#include <iostream>
#include <vector>
#include <algorithm>
#include <unordered_set>
#include <atomic>

using namespace std;

void printSearchStats(const SearchContext& ctx) {
    if (ctx.ttProbes > 0) {
        double hitRate = (double)ctx.ttHits / ctx.ttProbes * 100.0;
        string output = "STATS: Nodes=" + to_string(ctx.nodeCount) +
                        " TT=" + to_string(ctx.ttHits) + "/" + to_string(ctx.ttProbes) +
                        " (" + to_string(hitRate) + "%)";
        std::cerr << output << std::endl;
    }
}

int adjustMateScore(int score, int plyFromRoot) {
    if (score > MATE_THRESHOLD) {
        return score - plyFromRoot;  // Mate is closer to root
    } else if (score < -MATE_THRESHOLD) {
        return score + plyFromRoot;  // Mate is closer to root
    }
    return score;
}

int restoreMateScore(int score, int plyFromRoot) {
    if (score > MATE_THRESHOLD) {
        return score + plyFromRoot;
    } else if (score < -MATE_THRESHOLD) {
        return score - plyFromRoot;
    }
    return score;
}


int getTerminalValue(Game& game, const SearchContext& ctx) {
    if (game.isInCheck()) {
        return -MATE_VALUE + ctx.currentPly;
    }
    return 0;
}

int alphabeta(int alpha, int beta, int depth, Game& game, SearchContext& ctx) {
    ctx.nodeCount++;
    ++ctx.currentPly;

    if (ctx.nodeCount % 1024 == 0 && ctx.timeUp()) {
        int score = evalForSide(game, ctx);
        if (ctx.currentPly > 0) --ctx.currentPly;
        return score;
    }

    U64 hash = game.board.getHash();
    Move ttBestMove = MOVE_NONE;

    // check transposition table
    if (depth >= 0){
        int ttScore;
        ctx.ttProbes++;

        if (g_transpositionTable.probe(hash, alpha, beta, depth, ttScore)) {
            ctx.ttHits++;
            int ret = adjustMateScore(ttScore, ctx.currentPly);
            if (ctx.currentPly > 0) --ctx.currentPly;
            return ret;
        }
    }

    ttBestMove = g_transpositionTable.getBestMove(hash);

    if (game.isPositionTerminal()) {
        int score = getTerminalValue(game, ctx);
        if (depth > 0) {
            int adjustedScore = adjustMateScore(score, ctx.currentPly);
            g_transpositionTable.store(hash, adjustedScore, depth, TT_EXACT, MOVE_NONE);
        }
        if (ctx.currentPly > 0) --ctx.currentPly;
        return score;
    }

    if (depth <= 0) {
        int qs = quiescenceSearch(alpha, beta, game, 0, ctx);
        if (ctx.currentPly > 0) --ctx.currentPly;
        return qs;
    }

    if (game.isDrawByRule()) {
        if (ctx.currentPly > 0) --ctx.currentPly;
        return STALEMATE_VALUE;
    }

    // Generate legal moves
    MovesStruct legalMoves = game.generateAllLegalMoves();

    // no legal moves --> checkmate or stalemate
    if (legalMoves.getNumMoves() == 0) {
        int score = getTerminalValue(game, ctx);

        if (depth > 0) {
            int adjustedScore = adjustMateScore(score, ctx.currentPly);
            g_transpositionTable.store(hash, adjustedScore, depth, TT_EXACT,MOVE_NONE);
        }
        if (ctx.currentPly > 0) --ctx.currentPly;
        return score;
    }

    std::vector<std::pair<Move, int>> scoredMoves;
    scoredMoves.reserve(legalMoves.getNumMoves());

    for (int i = 0; i < legalMoves.getNumMoves(); ++i) {
        Move move = legalMoves.getMove(i);
        int moveScore = 0;

        if (ttBestMove == move){
            moveScore = 10000; // highest priority

        } else if (getCapturedPiece(move) != nEmpty) {
            // MVV-LVA scoring for captures
            int victim = pieceScore( getCapturedPiece(move) );
            int attacker = pieceScore( game.board.getPieceType(getFrom(move)) );

            // MVV-LVA scoring for remaining captures
            int victimScore = victim / 100;
            int attackerScore = attacker / 100;
            moveScore = 1000 + (victimScore * 10) - attackerScore;

        } else if (ctx.isKillerMove(move, depth)) {
            moveScore = 900;

        } else if (isPromotion(move) || isPromoCapture(move)) {
            moveScore = 800; // promotion bonus

        } else if (isKingCastle(move) || isQueenCastle(move)) {
            moveScore = 700; // castling bonus

        } else {
            // center control
            U8 to = getTo(move);
            if (to == 28 || to == 29 || to == 35 || to == 36) { // e4, e5, d4, d5
                moveScore = 100;
            }
        }

        scoredMoves.push_back({move, moveScore});
    }

    std::sort(scoredMoves.begin(), scoredMoves.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; }); // Sort by score descending

    int originalAlpha = alpha;
    int maxScore = -MATE_VALUE - 1; // worst possible score
    Move bestMove = scoredMoves[0].first; // Default to first legal move

    for (const auto& [move, moveScore] : scoredMoves) {

        if (ctx.timeUp()) break;

        game.pushMove(move);
        int score = -alphabeta(-beta, -alpha, depth - 1, game, ctx);
        game.popMove();

        if (score > maxScore) {
            maxScore = score;
            bestMove = move;
        }

        alpha = std::max(alpha, score);

        if (alpha >= beta) {
            if (getCapturedPiece(move) == nEmpty) {
                ctx.updateKillerMove(move, depth);
            }
            break; // Prune remaining moves
        }
    }

    if (depth >= 0) {
        TTFlag flag;
        if (maxScore <= originalAlpha) {
            flag = TT_UPPER; // Upper bound
        } else if (maxScore >= beta) {
            flag = TT_LOWER; // Lower bound
        } else {
            flag = TT_EXACT; // Exact score
        }
        int adjustedScore = adjustMateScore(maxScore, ctx.currentPly);
        g_transpositionTable.store(hash, adjustedScore, depth, flag, bestMove);
    }

    if (ctx.currentPly > 0) --ctx.currentPly;
    return maxScore;
}


Move searchAtDepth(Game& game, int depth, SearchContext& ctx, const std::vector<Move>* rootFilter) {

    MovesStruct legalMoves = game.generateAllLegalMoves();
    if (legalMoves.getNumMoves() == 0) return MOVE_NONE;

    std::unordered_set<U32> filterSet;
    if (rootFilter && !rootFilter->empty()) {
        filterSet.reserve(rootFilter->size());
        for (const auto& move : *rootFilter) {
            filterSet.insert(move);
        }
    }

    int alpha = -MATE_VALUE;
    int bestScore = -MATE_VALUE;
    int beta = MATE_VALUE;

    Move bestMove;
    bool foundMove = false;

    for (int i = 0; i < legalMoves.getNumMoves(); ++i) {
        if (ctx.timeUp()) break;

        Move move = legalMoves.getMove(i);
        if (!filterSet.empty() && filterSet.find(move) == filterSet.end()) {
            continue;
        }

        game.pushMove(move);
        int score = -alphabeta(-beta, -alpha, depth - 1, game, ctx);
        game.popMove();

        if (score > bestScore || !foundMove) {
            bestScore = score;
            bestMove = move;
            foundMove = true;
        }
    }
    return foundMove ? bestMove :MOVE_NONE;
}

int quiescenceSearch(int alpha, int beta, Game& game, int qDepth, SearchContext& ctx) {

    ctx.nodeCount++;
    if (ctx.timeUp()) return evalForSide(game, ctx);

    U64 hash = game.board.getHash();
    Move ttBestMove = MOVE_NONE;

    // Quiescence should not write negative depths into the TT.
    // Treat quiescence entries as depth 0 so they never appear deeper than main-search entries.
    int ttDepth = 0;
    int ttScore;

    ctx.ttProbes++;
    if (g_transpositionTable.probe(hash, alpha, beta, ttDepth, ttScore)) {
        ctx.ttHits++;
        return restoreMateScore(ttScore, ctx.currentPly);
    }

    ttBestMove = g_transpositionTable.getBestMove(hash);

    int originalAlpha = alpha;
    int standPat = evalForSide(game, ctx);

    const int DELTA_MARGIN = 900; // Queen value
    if (standPat + DELTA_MARGIN < alpha) {
        return standPat; // Position is so bad that even winning a queen won't help
    }

    if (standPat >= beta) {
        g_transpositionTable.store(hash, standPat, ttDepth, TT_LOWER,MOVE_NONE);
        return standPat;
    }

    if (standPat > alpha) alpha = standPat; // Update alpha

    MovesStruct captureMoves = game.generateAllLegalMoves(true); // Generate only capture moves);

    if (captureMoves.getNumMoves() == 0) {
        g_transpositionTable.store(hash, standPat, ttDepth, TT_EXACT,MOVE_NONE);
        return standPat;
    }

    std::vector<std::pair<Move, int>> scoredCaptures;
    scoredCaptures.reserve(captureMoves.getNumMoves());

    for (int i = 0; i < captureMoves.getNumMoves(); ++i) {
        Move move = captureMoves.getMove(i);
        int moveScore = 0;

        if (ttBestMove == move) {
            moveScore = 10000; // highest priority
        } else {
            // MVV-LVA scoring for captures
            int victim = pieceScore( getCapturedPiece(move) ) / 100;
            int attacker = pieceScore( game.board.getPieceType(getFrom(move)) ) / 100;
            moveScore = 1000 + (victim * 10) - attacker; // simple MVV-LVA heuristic
        }
        scoredCaptures.push_back({move, moveScore});
    }

    if (scoredCaptures.empty()) {
        g_transpositionTable.store(hash, standPat, 0, TT_EXACT,MOVE_NONE);
        return standPat;
    }

    std::sort(scoredCaptures.begin(), scoredCaptures.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; }); // Sort by score descending

    Move bestMove;
    bool foundMove = false;
    int bestScore = standPat;

    for (const auto& scoredCapture : scoredCaptures) {
        Move move = scoredCapture.first;

        if (ctx.timeUp()) break;

        game.pushMove(move);
        int score = -quiescenceSearch(-beta, -alpha, game, qDepth + 1, ctx);
        game.popMove();

        if (score >= beta) {
            g_transpositionTable.store(hash, score, ttDepth, TT_LOWER,MOVE_NONE);
            return score; // beta cutoff
        }

        if (score > bestScore) {
            bestScore = score;
            bestMove = move;
            foundMove = true;
        }

        if (score > alpha) {
            alpha = score; // Update alpha
        }
    }

    TTFlag flag;
    if (bestScore <= originalAlpha) {
        flag = TT_UPPER;  // Upper bound (no improvement)
    } else if (alpha >= beta) {
        flag = TT_LOWER;  // Lower bound (would have been cutoff)
    } else {
        flag = TT_EXACT;  // Exact score
    }

    int adjustedScore = adjustMateScore(bestScore, ctx.currentPly);
    Move storeMove = foundMove ? bestMove :MOVE_NONE;
    g_transpositionTable.store(hash, adjustedScore, ttDepth, flag, storeMove);

    return bestScore;
}
