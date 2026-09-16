#include "game_runner.h"

#include "../move.h"
#include "../game.h"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <unistd.h>

GameRunner::GameRunner(EngineInterface* engine1, EngineInterface* engine2,
                     const std::string& startFen,
                     const std::string& engine1Name,
                     const std::string& engine2Name)
    : engine1(engine1),
      engine2(engine2),
      startFen(startFen),
      engine1Name(engine1Name),
      engine2Name(engine2Name),
      game(startFen) {}

GameResult GameRunner::runGame(int movetimeMs) {
    GameResult result;
    result.startingFen = startFen;
    result.engineWhite = engine1Name;
    result.engineBlack = engine2Name;
    result.maxDepth = 0;
    result.totalNodes = 0;
    result.finalEvaluation = 0;

    const std::string fenToUse = (startFen == "startpos") ?
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1" : startFen;

    game.setPosition(fenToUse);

    if (engine1 != nullptr) {
        engine1->newGame();
    }
    if (engine2 != nullptr) {
        engine2->newGame();
    }

    if ((engine1 == nullptr || !engine1->setPosition(fenToUse)) ||
        (engine2 == nullptr || !engine2->setPosition(fenToUse))) {
        result.result = "*";
        result.reason = "Failed to initialize engine positions";
        return result;
    }

    std::vector<std::string> moveHistory;
    bool isWhite = (game.board.gameInfo & 1) == 0;
    const int maxMoves = 500;

    for (int ply = 1; ply <= maxMoves; ++ply) {
        const std::string termination = checkGameTermination();
        if (!termination.empty()) {
            result.result = termination;
            result.reason = "Game ended";
            break;
        }

        MovesStruct legalMoves = game.generateAllLegalMoves();
        if (legalMoves.getNumMoves() == 0) {
            const std::string noMoveTermination = checkGameTermination();
            if (!noMoveTermination.empty()) {
                result.result = noMoveTermination;
                result.reason = "No legal moves";
            } else if (game.isInCheck()) {
                result.result = isWhite ? "0-1" : "1-0";
                result.reason = "Checkmate";
            } else {
                result.result = "1/2-1/2";
                result.reason = "Stalemate";
            }
            break;
        }

        EngineInterface* currentEngine = getCurrentEngine(isWhite);
        const std::string currentEngineName = getCurrentEngineName(isWhite);
        if (currentEngine == nullptr || !currentEngine->setPosition(fenToUse, moveHistory)) {
            result.result = "*";
            result.reason = "Failed to set position for " + currentEngineName;
            break;
        }

        std::string bestMove;
        SearchStats stats;
        if (!currentEngine->getBestMove(movetimeMs, bestMove, stats)) {
            result.result = isWhite ? "0-1" : "1-0";
            result.reason = "Engine error: " + currentEngineName;
            break;
        }

        if (bestMove == "0000") {
            result.result = isWhite ? "0-1" : "1-0";
            result.reason = "Resignation: " + currentEngineName;
            break;
        }

        Move moveToApply = MOVE_NONE;
        bool moveFound = false;
        for (int i = 0; i < legalMoves.getNumMoves(); ++i) {
            const Move candidate = legalMoves.getMove(i);
            if (moveToString(candidate) == bestMove) {
                moveToApply = candidate;
                moveFound = true;
                break;
            }
        }

        if (!moveFound) {
            result.result = "*";
            result.reason = "Illegal move: " + bestMove;
            break;
        }

        MoveData moveData;
        moveData.uciMove = bestMove;
        moveData.sanMove = bestMove;
        moveData.depth = stats.depth;
        moveData.nodes = stats.nodes;
        moveData.evaluation = stats.score;
        moveData.moveNumber = ply;
        moveData.isWhite = isWhite;
        result.moves.push_back(moveData);

        if (stats.depth > result.maxDepth) {
            result.maxDepth = stats.depth;
        }
        result.totalNodes += stats.nodes;
        result.finalEvaluation = stats.score;

        game.pushMove(moveToApply);
        moveHistory.push_back(bestMove);

        const std::string immediateTermination = checkGameTermination();
        if (!immediateTermination.empty()) {
            result.result = immediateTermination;
            result.reason = "Game ended";
            break;
        }

        MovesStruct nextPlayerMoves = game.generateAllLegalMoves();
        if (nextPlayerMoves.getNumMoves() == 0) {
            if (game.isInCheck()) {
                result.result = isWhite ? "1-0" : "0-1";
            } else {
                result.result = "1/2-1/2";
            }
            result.reason = "No legal moves for next player";
            break;
        }

        isWhite = !isWhite;
    }

    if (result.result.empty()) {
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
    const GameState state = game.getGameState();

    switch (state) {
        case CHECKMATE:
            return (game.board.gameInfo & 1) ? "1-0" : "0-1";
        case STALEMATE:
        case DRAW_REPETITION:
        case DRAW_50_MOVE:
        case DRAW_INSUFFICIENT_MATERIAL:
            return "1/2-1/2";
        case ONGOING:
        default:
            return "";
    }
}

std::string GameRunner::getCurrentFen() {
    return game.board.toString();
}
