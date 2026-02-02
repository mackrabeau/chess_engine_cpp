#include "self_play.h"
#include "../movetables.h"
#include "../search.h"
#include <iostream>
#include <chrono>

namespace rl {

SelfPlayGenerator::SelfPlayGenerator(const SelfPlayConfig& cfg) : config(cfg) {
    // Initialize move tables
    MoveTables::instance().init();
    
    // Initialize NNUE if needed
    if (config.useNNUE) {
        evaluation::initializeNNUE(config.nnueModelPath);
        evaluation::setEvalMode(evaluation::EvalMode::NNUE);
    } else {
        evaluation::setEvalMode(evaluation::EvalMode::TRADITIONAL);
    }
    
    // Initialize game
    game.setPosition(config.startingFen);
    
    // Initialize NNUE accumulator if using NNUE
    if (config.useNNUE && evaluation::g_nnueNetwork != nullptr) {
        game.initializeNNUEAccumulator();
    }
}

TrainingGame SelfPlayGenerator::generateGame() {
    TrainingGame trainingGame;
    trainingGame.startingFen = config.startingFen;
    
    // Reset game to starting position
    game.setPosition(config.startingFen);
    if (config.useNNUE && evaluation::g_nnueNetwork != nullptr) {
        game.initializeNNUEAccumulator();
    }
    
    // Reset search stats
    resetSearchStats();
    setNodeLimit(-1);
    g_timeLimit = config.movetimeMs;
    g_searchStartTime = std::chrono::steady_clock::now();
    
    int moveNumber = 1;
    bool whiteToMove = true;
    
    while (moveNumber <= config.maxMoves) {
        // Check for game termination
        game.state = game.getGameState();
        if (game.state != ONGOING) {
            // Game ended
            switch (game.state) {
                case CHECKMATE:
                    trainingGame.result = whiteToMove ? "0-1" : "1-0";
                    trainingGame.reason = "checkmate";
                    break;
                case STALEMATE:
                    trainingGame.result = "1/2-1/2";
                    trainingGame.reason = "stalemate";
                    break;
                case DRAW_REPETITION:
                    trainingGame.result = "1/2-1/2";
                    trainingGame.reason = "repetition";
                    break;
                case DRAW_50_MOVE:
                    trainingGame.result = "1/2-1/2";
                    trainingGame.reason = "50move";
                    break;
                default:
                    trainingGame.result = "*";
                    trainingGame.reason = "unknown";
            }
            break;
        }
        
        // Check for legal moves
        auto legalMoves = game.generateAllLegalMoves();
        if (legalMoves.getNumMoves() == 0) {
            // No legal moves
            if (game.isInCheck()) {
                trainingGame.result = whiteToMove ? "0-1" : "1-0";
                trainingGame.reason = "checkmate";
            } else {
                trainingGame.result = "1/2-1/2";
                trainingGame.reason = "stalemate";
            }
            break;
        }
        
        // Record current position
        TrainingPosition pos;
        pos.fen = game.board.toString();
        pos.moveNumber = moveNumber;
        pos.isWhite = whiteToMove;
        
        // Search for best move
        resetSearchStats();
        resetStopSearchFlag();
        g_searchStartTime = std::chrono::steady_clock::now();
        
        Move bestMove = searchAtDepth(game, config.searchDepth);
        
        if (bestMove == MOVE_NONE) {
            // No move found, use first legal move
            if (legalMoves.getNumMoves() > 0) {
                bestMove = legalMoves.getMove(0);
            } else {
                break;
            }
        }
        
        // Get evaluation from search (evaluate current position)
        pos.searchDepth = config.searchDepth;
        pos.targetEval = static_cast<float>(evalForSide(game));
        pos.move = bestMove;
        
        // Store position (will be labeled with outcome later)
        trainingGame.positions.push_back(pos);
        
        // Make move
        game.pushMove(bestMove);
        
        whiteToMove = !whiteToMove;
        moveNumber++;
    }
    
    // If game didn't end naturally, mark as incomplete
    if (trainingGame.result == "*") {
        trainingGame.result = "*";
        trainingGame.reason = "incomplete";
    }
    
    // Label all positions with final game outcome
    labelPositionsWithOutcome(trainingGame, trainingGame.result);
    
    return trainingGame;
}

void SelfPlayGenerator::labelPositionsWithOutcome(TrainingGame& trainingGame, const std::string& result) {
    // Determine outcome value
    float outcomeValue = 0.0f;  // Draw
    if (result == "1-0") {
        outcomeValue = 10000.0f;  // White wins (in centipawns)
    } else if (result == "0-1") {
        outcomeValue = -10000.0f;  // Black wins
    }
    
    // Label each position
    // For positions where white is to move: use outcome directly
    // For positions where black is to move: negate outcome
    for (auto& pos : trainingGame.positions) {
        if (pos.isWhite) {
            // White to move: positive if white wins, negative if black wins
            pos.targetEval = outcomeValue;
        } else {
            // Black to move: negate (black's perspective)
            pos.targetEval = -outcomeValue;
        }
    }
}

std::vector<TrainingGame> SelfPlayGenerator::generateGames(int numGames) {
    std::vector<TrainingGame> games;
    games.reserve(numGames);
    
    for (int i = 0; i < numGames; ++i) {
        if ((i + 1) % 10 == 0) {
            std::cout << "Generated " << (i + 1) << " / " << numGames << " games\n";
        }
        games.push_back(generateGame());
    }
    
    return games;
}

int SelfPlayGenerator::getSearchEvaluation(Game& game, int /* depth */) {
    // This is a helper that could be used for more sophisticated evaluation
    // For now, we use evalForSide which is already called during search
    return evalForSide(game);
}

} // namespace rl
