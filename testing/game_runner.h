#ifndef GAME_RUNNER_H
#define GAME_RUNNER_H

#include "engine_interface.h"
#include "../game.h"

#include <string>
#include <vector>

struct MoveData {
    std::string uciMove;
    std::string sanMove;
    int depth = 0;
    long nodes = 0;
    int evaluation = 0;
    int moveNumber = 0;
    bool isWhite = false;
};

struct GameResult {
    std::string result;
    std::string reason;
    std::vector<MoveData> moves;
    std::string startingFen;
    std::string finalFen;
    std::string engineWhite;
    std::string engineBlack;
    int maxDepth = 0;
    long totalNodes = 0;
    int finalEvaluation = 0;
};

class GameRunner {
public:
    GameRunner(EngineInterface* engine1,
               EngineInterface* engine2,
               const std::string& startFen,
               const std::string& engine1Name,
               const std::string& engine2Name);

    GameResult runGame(int movetimeMs);

private:
    EngineInterface* engine1;
    EngineInterface* engine2;
    std::string startFen;
    std::string engine1Name;
    std::string engine2Name;
    Game game;

    EngineInterface* getCurrentEngine(bool isWhite);
    std::string getCurrentEngineName(bool isWhite);
    std::string checkGameTermination();
    std::string getCurrentFen();
};

#endif // GAME_RUNNER_H
