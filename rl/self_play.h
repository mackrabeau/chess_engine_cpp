#ifndef SELF_PLAY_H
#define SELF_PLAY_H

#include "../game.h"
#include "../search.h"
#include "../evaluation.h"
#include "../testing/engine_interface.h"

#include "training_data.h"
#include <string>
#include <vector>

namespace rl {

// Configuration for self-play
struct SelfPlayConfig {
    std::string enginePath = "./engine";
    int searchDepth = 4;              // Search depth for moves
    int maxMoves = 200;                // Maximum moves per game
    long movetimeMs = 1000;            // Time per move (ms) - not used if depth specified
    std::string startingFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    std::string nnueModelPath = "";    // Path to NNUE model (empty = random weights)
    
    SelfPlayConfig() {}
};

// Self-play game generator
class SelfPlayGenerator {
public:
    SelfPlayGenerator(const SelfPlayConfig& config);
    ~SelfPlayGenerator();
    
    // Generate a single game and return training data
    TrainingGame generateGame();
    
    // Generate multiple games
    std::vector<TrainingGame> generateGames(int numGames);
    
    // Get configuration
    const SelfPlayConfig& getConfig() const { return config; }
    
private:
    SelfPlayConfig config;
    Game game;
    
    // resuable engine instances
    std::unique_ptr<EngineInterface> engine1;
    std::unique_ptr<EngineInterface> engine2;
    bool enginesInitialized;

    // Helper to label positions with game outcome
    void labelPositionsWithOutcome(TrainingGame& trainingGame, const std::string& result);
    
    // Helper to get evaluation from search
    int getSearchEvaluation(Game& game, int depth);
};

} // namespace rl

#endif // SELF_PLAY_H
