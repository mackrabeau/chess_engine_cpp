#include "search.h"
#include "transposition.h"
#include <chrono>
#include <iostream>
#include <vector>
#include <algorithm>
#include <sstream>

using namespace std;

extern std::chrono::steady_clock::time_point g_searchStartTime;
extern long g_timeLimit;

long g_nodeCount = 0;
long g_ttHits = 0;
long g_ttProbes = 0;

bool g_timeoutOccurred = false;
Move killerMoves[MAX_SEARCH_DEPTH][2];

std::vector<std::string> g_searchTree;
int g_currentPly = 0;
bool g_recordSearchTree = false;
size_t g_searchTreeMaxLines = 200000;

static inline int getPlyFromRoot() {
    return g_currentPly > 0 ? g_currentPly : 0; // Ensure we don't return negative ply
}

// static inline int clampNonTerminalValue(int value) {
//     if (value > MATE_THRESHOLD) return MATE_THRESHOLD - 10;
//     if (value < -MATE_THRESHOLD) return -MATE_THRESHOLD + 10;
//     return value;
// }

void startSearchTree() {
    g_searchTree.clear();
    g_currentPly = 0;
    g_recordSearchTree = true;
}

void stopAndPrintSearchTree(size_t maxLines) {
    g_recordSearchTree = false;
    size_t printed = 0;
    for (const auto &line : g_searchTree) {
        if (printed++ >= maxLines) break;
        std::cerr << line << std::endl;
    }
}

// internal helpers
void recordEntry(const Game& game, int depth, int alpha, int beta) {
    if (g_recordSearchTree && g_searchTree.size() <= g_searchTreeMaxLines) {
        std::ostringstream oss;
        oss << std::string(g_currentPly * 2, ' ')
            << "ENT depth=" << depth
            << " ply=" << g_currentPly
            << " a=" << alpha << " b=" << beta
            << " hash=0x" << std::hex << game.board.getHash() << std::dec;
        // try to append a short FEN (if available) - keep it short to avoid massive lines
        oss << " fen=" << game.board.toString();
        g_searchTree.push_back(oss.str());
    }
    ++g_currentPly;
}

void recordExit(const Game& game, int depth, int score) {
    if (g_currentPly > 0) -- g_currentPly;
    if (g_recordSearchTree && g_searchTree.size() <= g_searchTreeMaxLines) {
        std::ostringstream oss;
        oss << std::string(g_currentPly * 2, ' ')
            << "EXIT depth=" << depth
            << " ply=" << g_currentPly
            << " score=" << score
            << " hash=0x" << std::hex << game.board.getHash() << std::dec;
        g_searchTree.push_back(oss.str());
    }
}


bool isTimeUp() {
    // return false;

    if (g_timeoutOccurred) return true;

    // check every 1024 nodes for efficiency
    if (g_nodeCount % 1024 == 0) {
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - g_searchStartTime).count();

        if (elapsedTime > g_timeLimit) {
            g_timeoutOccurred = true;  // Set flag only once
            return true;
        }
    }
    return false;
}

void resetSearchStats() {
    g_nodeCount = 0;
    g_ttHits = 0;
    g_ttProbes = 0;
    g_timeoutOccurred = false;

    for (int depth = 0; depth < MAX_SEARCH_DEPTH; ++depth) {
        killerMoves[depth][0] = Move();
        killerMoves[depth][1] = Move();
    }
}

void printSearchStats() {
    if (g_ttProbes > 0) {
        double hitRate = (double)g_ttHits / g_ttProbes * 100.0;
        string output = "STATS: Nodes=" + to_string(g_nodeCount) +
                        " TT=" + to_string(g_ttHits) + "/" + to_string(g_ttProbes) +
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


int getTerminalValue(Game& game, int depth) {
    int ply = getPlyFromRoot();
    if (game.isInCheck()) {
        return -MATE_VALUE + ply;
    }
    return 0;
}

int alphabeta(int alpha, int beta, int depth, Game& game){
    g_nodeCount++;
    recordEntry(game, depth, alpha, beta);

    if (g_nodeCount % 1024 == 0 && isTimeUp()) {
        int score = evalForSide(game);
        recordExit(game, depth, score);
        return score;
    }

    U64 hash = game.board.getHash();
    Move ttBestMove;

    // check transposition table
    if (depth >= 0){
        int ttScore;
        g_ttProbes++;

        if (g_transpositionTable.probe(hash, alpha, beta, depth, ttScore, ttBestMove)) {
            g_ttHits++;
            int ret = adjustMateScore(ttScore, getPlyFromRoot());
            recordExit(game, depth, ret);
            return ret;
        }
    }

    if (game.isPositionTerminal()) {
        int score = getTerminalValue(game, depth);
        if (depth > 0) {
            int adjustedScore = adjustMateScore(score, getPlyFromRoot());
            g_transpositionTable.store(hash, adjustedScore, depth, TT_EXACT, Move());
        }
        recordExit(game, depth, score);
        return score;
    }

    if (depth <= 0) {
        int qs = quiescenceSearch(alpha, beta, game, 0);
        recordExit(game, depth, qs);
        return qs;
    }

    // if (game.isFiftyMoveRule() || game.isThreefoldRepetition()) {
    if (game.isDrawByRule()) {
        recordExit(game, depth, STALEMATE_VALUE);
        return STALEMATE_VALUE;
    }

    // Generate legal moves
    MovesStruct legalMoves = game.generateAllLegalMoves();

    // no legal moves --> checkmate or stalemate
    if (legalMoves.getNumMoves() == 0) {
        int score = getTerminalValue(game, depth);

        if (depth > 0) {
            int adjustedScore = adjustMateScore(score, getPlyFromRoot());
            g_transpositionTable.store(hash, adjustedScore, depth, TT_EXACT, Move());
        }
        recordExit(game, depth, score);
        return score;
    }

    std::vector<std::pair<Move, int>> scoredMoves;
    scoredMoves.reserve(legalMoves.getNumMoves());

    for (int i = 0; i < legalMoves.getNumMoves(); ++i) {
        Move move = legalMoves.getMove(i);
        int moveScore = 0;

        if (ttBestMove.getMove() == move.getMove()){
            moveScore = 10000; // highest priority
            
        } else if (move.getCapturedPiece() != nEmpty) {
            // MVV-LVA scoring for captures
            int victim = pieceScore( move.getCapturedPiece() );
            int attacker = pieceScore( game.board.getPieceType(move.getFrom()) );

            // MVV-LVA scoring for remaining captures  
            int victimScore = victim / 100;
            int attackerScore = attacker / 100;
            moveScore = 1000 + (victimScore * 10) - attackerScore;

        } else if (isKillerMove(move, depth)) {
            moveScore = 900;

        } else if (move.isPromotion() || move.isPromoCapture()) {
            moveScore = 800; // promotion bonus

        } else if (move.isKingCastle() || move.isQueenCastle()) {
            moveScore = 700; // castling bonus

        } else {
            // center control
            U8 to = move.getTo();
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

        if (isTimeUp()) break; 

        game.pushMove(move);
        int score = -alphabeta(-beta, -alpha, depth - 1, game);
        game.popMove();

        if (score > maxScore) {
            maxScore = score;
            bestMove = move;
        }

        alpha = std::max(alpha, score);

        if (alpha >= beta) {
            if (move.getCapturedPiece() == nEmpty) {
                updateKillerMove(move, depth); // Update killer move
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
        int adjustedScore = adjustMateScore(maxScore, getPlyFromRoot());
        g_transpositionTable.store(hash, adjustedScore, depth, flag, bestMove);
    }

    recordExit(game, depth, maxScore);
    return maxScore;
}


Move searchAtDepth(Game& game, int depth) {

    MovesStruct legalMoves = game.generateAllLegalMoves();
    if (legalMoves.getNumMoves() == 0) return Move();
    
    int alpha = -MATE_VALUE;
    int bestScore = -MATE_VALUE;
    int beta = MATE_VALUE;

    Move bestMove;

    for (int i = 0; i < legalMoves.getNumMoves(); ++i) {
        if (isTimeUp()) break;
        
        Move move = legalMoves.getMove(i);
        
        game.pushMove(move);
        int score = -alphabeta(-beta, -alpha, depth - 1, game);
        game.popMove();

        if (score > bestScore) {
            bestScore = score;
            bestMove = move;
        }
    }
    return bestMove;
}

void updateKillerMove(Move move, int depth) {
    if (killerMoves[depth][0].getMove() != move.getMove()) {
        killerMoves[depth][1] = killerMoves[depth][0]; 
        killerMoves[depth][0] = move;
    }
}

bool isKillerMove(Move move, int depth) {
    if (depth < 0 || depth >= MAX_SEARCH_DEPTH) return false;
    return (killerMoves[depth][0].getMove() == move.getMove() || killerMoves[depth][1].getMove() == move.getMove());
}

int quiescenceSearch(int alpha, int beta, Game& game, int qDepth) {

    g_nodeCount++;
    if (isTimeUp()) return evalForSide(game);

    // if (qDepth > 8) {
    //     return evalForSide(game);
    // }

    U64 hash = game.board.getHash();
    Move ttBestMove;

    // Quiescence should not write negative depths into the TT.
    // Treat quiescence entries as depth 0 so they never appear deeper than main-search entries.
    int ttDepth = 0;
    int ttScore;

    g_ttProbes++;
    if (g_transpositionTable.probe(hash, alpha, beta, ttDepth, ttScore, ttBestMove)) {
        g_ttHits++;
        return restoreMateScore(ttScore, getPlyFromRoot());
    }

    int originalAlpha = alpha;
    int standPat = evalForSide(game);

    const int DELTA_MARGIN = 900; // Queen value
    if (standPat + DELTA_MARGIN < alpha) {
        return standPat; // Position is so bad that even winning a queen won't help
    }

    if (standPat >= beta) {
        g_transpositionTable.store(hash, standPat, ttDepth, TT_LOWER, Move());
        return standPat;
    }

    if (standPat > alpha) alpha = standPat; // Update alpha

    MovesStruct captureMoves = game.generateAllLegalMoves(true); // Generate only capture moves);

    if (captureMoves.getNumMoves() == 0) {
        g_transpositionTable.store(hash, standPat, ttDepth, TT_EXACT, Move());
        return standPat;
    }

    std::vector<std::pair<Move, int>> scoredCaptures;
    scoredCaptures.reserve(captureMoves.getNumMoves());

    for (int i = 0; i < captureMoves.getNumMoves(); ++i) {
        Move move = captureMoves.getMove(i);
        int moveScore = 0;

        if (ttBestMove.getMove() == move.getMove()) {
            moveScore = 10000; // highest priority
        } else {
            // MVV-LVA scoring for captures
            int victim = pieceScore( move.getCapturedPiece() ) / 100;
            int attacker = pieceScore( game.board.getPieceType(move.getFrom()) ) / 100;
            moveScore = 1000 + (victim * 10) - attacker; // simple MVV-LVA heuristic
        }
        scoredCaptures.push_back({move, moveScore});
    }   

    if (scoredCaptures.empty()) {
        g_transpositionTable.store(hash, standPat, 0, TT_EXACT, Move());
        return standPat;
    }

    std::sort(scoredCaptures.begin(), scoredCaptures.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; }); // Sort by score descending
    
    Move bestMove;
    bool foundMove = false;
    int bestScore = standPat;

    for (const auto& scoredCapture : scoredCaptures) {
        Move move = scoredCapture.first;

        if (isTimeUp()) break;

        // SEE(Static Exchange Evaluation) pruning
        // int victim = pieceScore(move.getCapturedPiece()) / 100;
        // int attacker = pieceScore(game.board.getPieceType(move.getFrom())) / 100;
        // int see = (victim * 100) - (attacker * 100); // Simple SEE approximation

        // // Skip obviously bad captures (losing more than we gain)
        // if (see < -100) {
        //     continue;
        // }

        game.pushMove(move);
        int score = -quiescenceSearch(-beta, -alpha, game, qDepth + 1);
        game.popMove();

        if (score >= beta) {
            g_transpositionTable.store(hash, score, ttDepth, TT_LOWER, Move());
            return score; // beta cutoff
        }

        if (score > bestScore) {
            bestScore = score;
            bestMove = move;
            foundMove = true;
        }

        if (score > alpha) {
            alpha = score; // Update alpha
            // bestMove = move;
            // foundMove = true;
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

    int adjustedScore = adjustMateScore(bestScore, getPlyFromRoot());
    Move storeMove = foundMove ? bestMove : Move();
    g_transpositionTable.store(hash, adjustedScore, ttDepth, flag, storeMove);

    return bestScore;
}