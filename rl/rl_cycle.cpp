#include "rl_cycle.h"
#include "../evaluation.h"
#include "../search.h"
#include "../game.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>

namespace rl {

RLCycle::RLCycle(const RLCycleConfig& cfg) : config(cfg), currentCycle(0) {
    // Initialize bestModelPath from config
    bestModelPath = config.modelPath;
    if (bestModelPath.empty()) {
        bestModelPath = "rl_model.bin";
    }
}

TrainingDataset RLCycle::generateSelfPlayGames() {
    std::cout << "\n=== Generating Self-Play Games ===\n";
    
    SelfPlayConfig spConfig;
    spConfig.searchDepth = config.selfPlaySearchDepth;
    spConfig.maxMoves = config.selfPlayMaxMoves;
    spConfig.useNNUE = true;
    spConfig.nnueModelPath = bestModelPath;  // Use current best model
    
    SelfPlayGenerator generator(spConfig);
    auto games = generator.generateGames(config.selfPlayGames);
    
    TrainingDataset dataset;
    for (const auto& game : games) {
        dataset.addGame(game);
    }
    
    std::cout << "Generated " << dataset.getGameCount() << " games with "
             << dataset.getPositionCount() << " positions.\n";
    
    return dataset;
}

bool RLCycle::trainModel(const TrainingDataset& dataset, const std::string& outputPath) {
    std::cout << "\n=== Training Model ===\n";
    
    // Load current best model or create new one
    nnue::NNUE* network = new nnue::NNUE();
    
    // Try to load existing model
    if (!bestModelPath.empty()) {
        std::ifstream testFile(bestModelPath);
        if (testFile.good()) {
            testFile.close();
            if (!network->loadModel(bestModelPath)) {
                std::cout << "Warning: Could not load existing model, starting from scratch.\n";
            } else {
                std::cout << "Loaded model from " << bestModelPath << "\n";
            }
        }
    }
    
    // Configure training
    TrainingConfig trainConfig;
    trainConfig.learningRate = config.trainingLearningRate;
    trainConfig.batchSize = config.trainingBatchSize;
    trainConfig.numEpochs = config.trainingEpochs;
    trainConfig.validationSplit = 0.1f;
    trainConfig.useAdam = config.useAdam;
    trainConfig.saveInterval = 0;  // Don't save during training
    trainConfig.savePath = outputPath;
    
    // Train (need non-const reference, so create a copy)
    TrainingDataset datasetCopy = dataset;
    NNUETrainer trainer(network, trainConfig);
    trainer.train(datasetCopy);
    
    delete network;
    return true;
}

float RLCycle::evaluateModel(const std::string& modelPath, const std::string& previousModelPath) {
    std::cout << "\n=== Evaluating Model ===\n";
    std::cout << "New model: " << modelPath << "\n";
    std::cout << "Previous model: " << previousModelPath << "\n";
    
    // Use test_runner to play games between the two models
    // We'll use a simple approach: run test_runner as a subprocess
    
    // Create wrapper scripts for this evaluation
    std::string newModelScript = "eval_new_model.sh";
    std::string prevModelScript = "eval_prev_model.sh";
    
    // Create wrapper scripts
    std::ofstream newScript(newModelScript);
    newScript << "#!/bin/bash\n";
    newScript << "export EVAL_MODE=nnue\n";
    newScript << "export NNUE_MODEL=" << modelPath << "\n";
    newScript << "exec ./engine \"$@\"\n";
    newScript.close();
    
    std::ofstream prevScript(prevModelScript);
    prevScript << "#!/bin/bash\n";
    prevScript << "export EVAL_MODE=nnue\n";
    prevScript << "export NNUE_MODEL=" << previousModelPath << "\n";
    prevScript << "exec ./engine \"$@\"\n";
    prevScript.close();
    
    chmod(newModelScript.c_str(), 0755);
    chmod(prevModelScript.c_str(), 0755);
    
    // Run test_runner
    std::string testRunnerCmd = "./test_runner " + newModelScript + " \"New Model\" " +
                                prevModelScript + " \"Previous Model\" " +
                                "--movetime " + std::to_string(config.evalMovetimeMs) +
                                " --num-games " + std::to_string(config.evalGames) +
                                " --swap-colors";
    
    std::cout << "Running evaluation: " << testRunnerCmd << "\n";
    
    int result = system(testRunnerCmd.c_str());
    
    // Clean up scripts
    unlink(newModelScript.c_str());
    unlink(prevModelScript.c_str());
    
    if (result != 0) {
        std::cerr << "Warning: Evaluation test failed\n";
        return 0.5f;  // Assume draw if evaluation fails
    }
    
    // Parse results from CSV
    std::string csvPath = "./test_results/games.csv";
    std::ifstream csv(csvPath);
    if (!csv.is_open()) {
        std::cerr << "Warning: Could not read evaluation results\n";
        return 0.5f;
    }
    
    // Count wins for new model
    int newModelWins = 0;
    int totalGames = 0;
    std::string line;
    std::getline(csv, line);  // Skip header
    
    while (std::getline(csv, line)) {
        if (line.empty()) continue;
        
        // Parse CSV: result,engine_white,engine_black,...
        size_t firstComma = line.find(',');
        if (firstComma == std::string::npos) continue;
        
        std::string result = line.substr(0, firstComma);
        std::string rest = line.substr(firstComma + 1);
        
        size_t secondComma = rest.find(',');
        if (secondComma == std::string::npos) continue;
        
        std::string whiteEngine = rest.substr(0, secondComma);
        
        // Check if new model won
        if (whiteEngine == "New Model") {
            if (result == "1-0") newModelWins++;
            totalGames++;
        } else {
            // New model is black
            if (result == "0-1") newModelWins++;
            totalGames++;
        }
    }
    
    csv.close();
    
    if (totalGames == 0) {
        std::cerr << "Warning: No games found in results\n";
        return 0.5f;
    }
    
    float winRate = static_cast<float>(newModelWins) / totalGames;
    std::cout << "New model win rate: " << std::fixed << std::setprecision(2) 
             << (winRate * 100.0f) << "% (" << newModelWins << "/" << totalGames << ")\n";
    
    return winRate;
}

bool RLCycle::updateModelIfBetter(const std::string& newModelPath, float winRate) {
    std::cout << "\n=== Model Update Decision ===\n";
    std::cout << "Win rate: " << std::fixed << std::setprecision(2) << (winRate * 100.0f) << "%\n";
    std::cout << "Threshold: " << (config.improvementThreshold * 100.0f) << "%\n";
    
    if (winRate >= config.improvementThreshold) {
        std::cout << "New model is better! Updating...\n";
        
        // Backup current best model
        if (!bestModelPath.empty()) {
            std::ifstream src(bestModelPath, std::ios::binary);
            if (src.good()) {
                std::ofstream dst(config.backupModelPath, std::ios::binary);
                dst << src.rdbuf();
                dst.close();
                src.close();
                std::cout << "Backed up previous model to " << config.backupModelPath << "\n";
            }
        }
        
        // Update best model path
        bestModelPath = newModelPath;
        config.modelPath = newModelPath;
        std::cout << "New model accepted as best model.\n";
        return true;
    } else {
        std::cout << "New model is not better. Keeping previous model.\n";
        return false;
    }
}

bool RLCycle::runCycle(int cycleNumber) {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "RL Cycle " << cycleNumber << "\n";
    std::cout << "========================================\n";
    
    currentCycle = cycleNumber;
    
    // Step 1: Generate self-play games
    TrainingDataset dataset = generateSelfPlayGames();
    
    if (dataset.getPositionCount() == 0) {
        std::cerr << "Error: No training data generated\n";
        return false;
    }
    
    // Save dataset
    std::string datasetPath = "rl_cycle_" + std::to_string(cycleNumber) + "_data.bin";
    if (!dataset.save(datasetPath)) {
        std::cerr << "Warning: Could not save training dataset\n";
    }
    
    // Step 2: Train model
    std::string newModelPath = "rl_cycle_" + std::to_string(cycleNumber) + "_model.bin";
    if (!trainModel(dataset, newModelPath)) {
        std::cerr << "Error: Training failed\n";
        return false;
    }
    
    // Step 3: Evaluate model
    std::string previousModelPath = bestModelPath;
    
    // If this is the first cycle, we need a previous model to compare against
    if (previousModelPath.empty() || access(previousModelPath.c_str(), F_OK) != 0) {
        std::cout << "No previous model found. Using new model as baseline.\n";
        bestModelPath = newModelPath;
        return true;
    }
    
    float winRate = evaluateModel(newModelPath, previousModelPath);
    
    // Step 4: Update if better
    updateModelIfBetter(newModelPath, winRate);
    
    return true;
}

void RLCycle::runCycles() {
    std::cout << "=== Starting RL Training Cycles ===\n";
    std::cout << "Configuration:\n";
    std::cout << "  Self-play games per cycle: " << config.selfPlayGames << "\n";
    std::cout << "  Training epochs per cycle: " << config.trainingEpochs << "\n";
    std::cout << "  Evaluation games: " << config.evalGames << "\n";
    std::cout << "  Improvement threshold: " << (config.improvementThreshold * 100.0f) << "%\n";
    std::cout << "  Max cycles: " << (config.maxCycles > 0 ? std::to_string(config.maxCycles) : "infinite") << "\n\n";
    
    int cycle = 1;
    while (config.maxCycles == 0 || cycle <= config.maxCycles) {
        if (!runCycle(cycle)) {
            std::cerr << "Cycle " << cycle << " failed. Stopping.\n";
            break;
        }
        
        cycle++;
        
        // Small delay between cycles
        usleep(100000);  // 100ms
    }
    
    std::cout << "\n=== RL Training Complete ===\n";
    std::cout << "Best model: " << bestModelPath << "\n";
}

} // namespace rl
