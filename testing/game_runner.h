#ifndef GAME_RUNNER_H
#define GAME_RUNNER_H

#include "engine_interface.h"
#include "../game.h"
#include <string>
#include <vector>

struct MoveData {
    std::string uciMove;
    std::string sanMove;
    Move move;
    int depth;
    long nodes;
    int evaluation;
    int moveNumber;
    bool isWhite;
};

struct GameResult {
    std::string result;  // "1-0", "0-1", "1/2-1/2"
    std::string reason;  // "checkmate", "stalemate", "draw", etc.
    std::vector<MoveData> moves;
    std::string startingFen;
    std::string finalFen;
    std::string engineWhite;
    std::string engineBlack;
    int maxDepth;
    long totalNodes;
    int finalEvaluation;
};

class GameRunner {
public:
    GameRunner(EngineInterface* engine1, EngineInterface* engine2, 
               const std::string& startFen, const std::string& engine1Name, const std::string& engine2Name);
    
    // Run the game and return result
    GameResult runGame(int movetimeMs);
    
private:
    EngineInterface* engine1;
    EngineInterface* engine2;
    std::string startFen;
    std::string engine1Name;
    std::string engine2Name;
    Game game;
    
    // Helper to get current engine (alternating)
    EngineInterface* getCurrentEngine(bool isWhite);
    std::string getCurrentEngineName(bool isWhite);
    
    // Check game termination
    std::string checkGameTermination();
    
    // Get FEN string from game
    std::string getCurrentFen();
};

#endif // GAME_RUNNER_H
