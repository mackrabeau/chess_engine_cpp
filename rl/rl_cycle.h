#ifndef RL_CYCLE_H
#define RL_CYCLE_H

#include "self_play.h"
#include "training_data.h"
#include "trainer.h"
#include "../nnue.h"
#include <string>

namespace rl {

// Configuration for RL training cycle
struct RLCycleConfig {
    // Self-play configuration
    int selfPlayGames = 100;           // Number of games to generate per cycle
    int selfPlaySearchDepth = 3;        // Search depth for self-play
    int selfPlayMaxMoves = 200;         // Max moves per game
    
    // Training configuration
    int trainingEpochs = 10;            // Epochs to train per cycle
    int trainingBatchSize = 256;         // Batch size for training
    float trainingLearningRate = 0.001f; // Learning rate
    bool useAdam = true;                // Use Adam optimizer
    
    // Evaluation configuration
    int evalGames = 20;                  // Number of games to play for evaluation
    int evalSearchDepth = 4;             // Search depth for evaluation
    int evalMovetimeMs = 1000;           // Time per move for evaluation
    
    // Model management
    std::string modelPath = "rl_model.bin";        // Current model path
    std::string previousModelPath = "rl_model_prev.bin"; // Previous model path
    std::string backupModelPath = "rl_model_backup.bin";  // Backup model path
    
    // Cycle control
    int maxCycles = 10;                  // Maximum number of cycles (0 = infinite)
    float improvementThreshold = 0.55f;  // Win rate needed to accept new model (0.55 = 55%)
    
    RLCycleConfig() {}
};

// RL Training Cycle Manager
class RLCycle {
public:
    RLCycle(const RLCycleConfig& config);
    
    // Run a single cycle: self-play -> train -> evaluate -> update
    bool runCycle(int cycleNumber);
    
    // Run multiple cycles
    void runCycles();
    
    // Get current cycle number
    int getCurrentCycle() const { return currentCycle; }
    
    // Get best model path
    std::string getBestModelPath() const { return bestModelPath; }
    
private:
    RLCycleConfig config;
    int currentCycle;
    std::string bestModelPath;
    
    // Generate self-play games
    TrainingDataset generateSelfPlayGames();
    
    // Train model on dataset
    bool trainModel(const TrainingDataset& dataset, const std::string& outputPath);
    
    // Evaluate model by playing against previous version
    float evaluateModel(const std::string& modelPath, const std::string& previousModelPath);
    
    // Update model if better
    bool updateModelIfBetter(const std::string& newModelPath, float winRate);
};

} // namespace rl

#endif // RL_CYCLE_H
