#include "self_play.h"
#include "../testing/game_runner.h"
#include "../testing/engine_interface.h"
#include "../movetables.h"
#include "../search.h"
#include <iostream>
#include <memory>

namespace rl {

SelfPlayGenerator::SelfPlayGenerator(const SelfPlayConfig& cfg) : config(cfg) {
    // Initialize move tables
    MoveTables::instance().init();

    if (!config.nnueModelPath.empty()) {
        setenv("NNUE_MODEL", config.nnueModelPath.c_str(), 1);
    }
    setenv("EVAL_MODE", "nnue", 1);

    // Create engine instances once (will be reused for all games)
    engine1 = std::make_unique<EngineInterface>(config.enginePath, "SelfPlay-White");
    engine2 = std::make_unique<EngineInterface>(config.enginePath, "SelfPlay-Black");
    
    // Initialize engines once
    if (engine1->initialize() && engine2->initialize()) {
        enginesInitialized = true;
    } else {
        std::cerr << "Error: Failed to initialize engines for self-play\n";
        enginesInitialized = false;
    }
}

SelfPlayGenerator::~SelfPlayGenerator() {
    // Clean up engines
    if (engine1) {
        engine1->quit();
    }
    if (engine2) {
        engine2->quit();
    }
}

TrainingGame SelfPlayGenerator::generateGame() {
    TrainingGame trainingGame;
    trainingGame.startingFen = config.startingFen;
    
    if (!enginesInitialized) {
        std::cerr << "Error: Failed to initialize engines for self-play\n";
        trainingGame.result = "*";
        trainingGame.reason = "engine_init_failed";
        return trainingGame;
    }

    engine1->newGame();
    engine2->newGame();


    std::cout << "  Starting game..." << std::flush;
    GameRunner runner(engine1.get(), engine2.get(), config.startingFen, "SelfPlay-White", "SelfPlay-Black");
    GameResult gameResult = runner.runGame(config.movetimeMs);
    
    std::cout << " finished: " << gameResult.result 
              << " (" << gameResult.moves.size() << " moves)" << std::endl;
    
    trainingGame.result = gameResult.result;
    trainingGame.reason = gameResult.reason;

    Game game;
    game.setPosition(config.startingFen);

    for (const auto& moveData : gameResult.moves) {
        TrainingPosition pos;

        pos.featuresWhite = nnue::FeatureExtractor::extractFeatures(game.board, 0);
        pos.featuresBlack = nnue::FeatureExtractor::extractFeatures(game.board, 1);

        pos.moveNumber = moveData.moveNumber;
        pos.isWhite = moveData.isWhite;
        pos.searchDepth = moveData.depth;
        pos.targetEval = static_cast<float>(moveData.evaluation) / 100.0f;
        pos.move = moveData.move;  
        
        trainingGame.positions.push_back(pos);
        
        game.pushMove(moveData.move);
    }

    // if (trainingGame.result != "*") {
    //     labelPositionsWithOutcome(trainingGame, trainingGame.result);
    // }

    return trainingGame;

}

void SelfPlayGenerator::labelPositionsWithOutcome(TrainingGame& trainingGame, const std::string& result) {
    return;
    // // Determine outcome value
    // float outcomeValue = 0.0f;  // Draw
    // if (result == "1-0") {
    //     outcomeValue = 10000.0f;  // White wins (in centipawns)
    // } else if (result == "0-1") {
    //     outcomeValue = -10000.0f;  // Black wins
    // }
    
    // // Label each position
    // // For positions where white is to move: use outcome directly
    // // For positions where black is to move: negate outcome
    // for (auto& pos : trainingGame.positions) {
    //     if (pos.isWhite) {
    //         // White to move: positive if white wins, negative if black wins
    //         pos.targetEval = outcomeValue;
    //     } else {
    //         // Black to move: negate (black's perspective)
    //         pos.targetEval = -outcomeValue;
    //     }
    // }
}

std::vector<TrainingGame> SelfPlayGenerator::generateGames(int numGames) {
    std::vector<TrainingGame> games;
    games.reserve(numGames);
    
    for (int i = 0; i < numGames; ++i) {
        std::cout << "Game " << (i + 1) << " / " << numGames << ": " << std::flush;
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
