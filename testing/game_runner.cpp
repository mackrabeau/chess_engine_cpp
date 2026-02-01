#include "game_runner.h"
#include "san_converter.h"
#include "../move.h"
#include "../game.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <unistd.h>

GameRunner::GameRunner(EngineInterface* engine1, EngineInterface* engine2,
                       const std::string& startFen, const std::string& engine1Name, const std::string& engine2Name)
    : engine1(engine1), engine2(engine2), startFen(startFen), engine1Name(engine1Name), engine2Name(engine2Name), game(startFen)
{
}

GameResult GameRunner::runGame(int movetimeMs) {
    GameResult result;
    result.startingFen = startFen;
    result.engineWhite = engine1Name;
    result.engineBlack = engine2Name;
    result.maxDepth = 0;
    result.totalNodes = 0;
    result.finalEvaluation = 0;
    
    // Set initial position
    std::string fenToUse = (startFen == "startpos") ? 
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1" : startFen;
    
    // Reinitialize game with the correct FEN
    game.setPosition(fenToUse);
    
    // Send ucinewgame to reset engine state
    engine1->newGame();
    engine2->newGame();
    
    // Small delay to ensure ucinewgame is processed
    usleep(50000); // 50ms
    
    if (!engine1->setPosition(fenToUse) || !engine2->setPosition(fenToUse)) {
        result.result = "*";
        result.reason = "Failed to set position";
        return result;
    }
    
    std::vector<std::string> moveHistory;
    int moveNumber = 1;
    bool isWhite = (game.board.gameInfo & 1) == 0; // Check if white to move
    
    const int MAX_MOVES = 500; // Prevent infinite games
    
    while (moveNumber <= MAX_MOVES) {
        // Check for game termination BEFORE asking for a move
        // This prevents asking for moves when the game is already over
        std::string termination = checkGameTermination();
        if (!termination.empty()) {
            result.result = termination;
            result.reason = "Game ended";
            break;
        }
        
        // Also check if there are any legal moves - if not, game is over
        MovesStruct legalMovesCheck = game.generateAllLegalMoves();
        if (legalMovesCheck.getNumMoves() == 0) {
            // No legal moves - game must be over (checkmate or stalemate)
            termination = checkGameTermination();
            if (!termination.empty()) {
                result.result = termination;
                result.reason = "Game ended (no legal moves)";
            } else {
                // Fallback - determine result based on check
                if (game.isInCheck()) {
                    result.result = isWhite ? "0-1" : "1-0"; // Checkmate
                } else {
                    result.result = "1/2-1/2"; // Stalemate
                }
                result.reason = "Game ended (no legal moves)";
            }
            break;
        }
        
        // Get current engine
        EngineInterface* currentEngine = getCurrentEngine(isWhite);
        std::string currentEngineName = getCurrentEngineName(isWhite);
        
        // Ensure the current engine has the correct position before asking for a move
        // This is critical - the engine must have the exact current position
        if (!currentEngine->setPosition(fenToUse, moveHistory)) {
            result.result = "*";
            result.reason = "Failed to set position for " + currentEngineName;
            break;
        }
        
        // Small delay to ensure position is processed
        usleep(10000); // 10ms
        
        // Get best move
        std::string bestMove;
        SearchStats stats;
        
        if (!currentEngine->getBestMove(movetimeMs, bestMove, stats)) {
            result.result = isWhite ? "0-1" : "1-0";
            result.reason = "Engine error: " + currentEngineName;
            break;
        }
        
        // Check for resignation
        if (bestMove == "0000") {
            result.result = isWhite ? "0-1" : "1-0";
            result.reason = "Resignation: " + currentEngineName;
            break;
        }
        
        // Apply move to game
        MovesStruct legalMoves = game.generateAllLegalMoves();
        bool moveFound = false;
        Move moveToApply = MOVE_NONE;
        
        // Debug: log current position and legal moves if needed
        if (legalMoves.getNumMoves() == 0) {
            result.result = "*";
            result.reason = "No legal moves available";
            break;
        }
        
        for (int i = 0; i < legalMoves.getNumMoves(); ++i) {
            Move move = legalMoves.getMove(i);
            if (moveToString(move) == bestMove) {
                moveToApply = move;
                moveFound = true;
                break;
            }
        }
        
        if (!moveFound) {
            // Try to get current FEN for debugging
            std::string currentFen = game.board.toString();
            std::cerr << "Illegal move: " << bestMove << " from engine " << currentEngineName << std::endl;
            std::cerr << "Current position: " << currentFen << std::endl;
            std::cerr << "Legal moves count: " << legalMoves.getNumMoves() << std::endl;
            if (legalMoves.getNumMoves() > 0 && legalMoves.getNumMoves() <= 5) {
                std::cerr << "Legal moves: ";
                for (int i = 0; i < legalMoves.getNumMoves(); ++i) {
                    std::cerr << moveToString(legalMoves.getMove(i)) << " ";
                }
                std::cerr << std::endl;
            }
            result.result = "*";
            result.reason = "Illegal move: " + bestMove;
            break;
        }
        
        // Convert to SAN
        std::string sanMove = SanConverter::uciToSan(bestMove, game);
        
        // Record move data
        MoveData moveData;
        moveData.uciMove = bestMove;
        moveData.sanMove = sanMove;
        moveData.depth = stats.depth;
        moveData.nodes = stats.nodes;
        moveData.evaluation = stats.score;
        moveData.moveNumber = moveNumber;
        moveData.isWhite = isWhite;
        result.moves.push_back(moveData);
        
        // Update aggregates
        if (stats.depth > result.maxDepth) {
            result.maxDepth = stats.depth;
        }
        result.totalNodes += stats.nodes;
        result.finalEvaluation = stats.score;
        
        // Apply move to our game state first
        game.pushMove(moveToApply);
        moveHistory.push_back(bestMove);
        
        // Check for immediate termination after applying the move
        // This catches checkmate/stalemate right away
        std::string immediateTermination = checkGameTermination();
        if (!immediateTermination.empty()) {
            result.result = immediateTermination;
            result.reason = "Game ended";
            break;
        }
        
        // Also check if there are no legal moves for the next player
        MovesStruct nextPlayerMoves = game.generateAllLegalMoves();
        if (nextPlayerMoves.getNumMoves() == 0) {
            // Next player has no moves - game is over
            if (game.isInCheck()) {
                // Checkmate - current player (who just moved) wins
                result.result = isWhite ? "1-0" : "0-1";
            } else {
                // Stalemate
                result.result = "1/2-1/2";
            }
            result.reason = "Game ended (no legal moves for next player)";
            break;
        }
        
        // Note: We don't update engine positions here anymore - we do it at the start
        // of the next iteration to ensure the correct engine has the correct position
        
        // Switch sides
        isWhite = !isWhite;
        if (isWhite) {
            moveNumber++;
        }
    }
    
    if (moveNumber > MAX_MOVES) {
        result.result = "1/2-1/2";
        result.reason = "Maximum moves reached";
    }
    
    result.finalFen = getCurrentFen();
    
    return result;
}

EngineInterface* GameRunner::getCurrentEngine(bool isWhite) {
    return isWhite ? engine1 : engine2;
}

std::string GameRunner::getCurrentEngineName(bool isWhite) {
    return isWhite ? engine1Name : engine2Name;
}

std::string GameRunner::checkGameTermination() {
    GameState state = game.getGameState();
    
    switch (state) {
        case CHECKMATE:
            // Checkmate - opposite side wins
            return (game.board.gameInfo & 1) ? "1-0" : "0-1";
        case STALEMATE:
        case DRAW_REPETITION:
        case DRAW_50_MOVE:
        case DRAW_INSUFFICIENT_MATERIAL:
            return "1/2-1/2";
        case ONGOING:
            return "";
        default:
            return "";
    }
}

std::string GameRunner::getCurrentFen() {
    return game.board.toString();
}
