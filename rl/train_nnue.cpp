#include "trainer.h"
#include "self_play.h"
#include "training_data.h"
#include "../nnue.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    std::cout << "=== NNUE Training Program ===\n\n";
    
    // Parse command line arguments
    std::string datasetPath = "";
    std::string modelPath = "";
    std::string outputModelPath = "trained_model.bin";
    int numGames = 0;
    bool generateData = false;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--dataset" && i + 1 < argc) {
            datasetPath = argv[++i];
        } else if (arg == "--model" && i + 1 < argc) {
            modelPath = argv[++i];
        } else if (arg == "--output" && i + 1 < argc) {
            outputModelPath = argv[++i];
        } else if (arg == "--generate" && i + 1 < argc) {
            numGames = std::stoi(argv[++i]);
            generateData = true;
        } else if (arg == "--help") {
            std::cout << "Usage: train_nnue [options]\n";
            std::cout << "Options:\n";
            std::cout << "  --dataset <path>    Load training data from file\n";
            std::cout << "  --model <path>      Load initial model from file (optional)\n";
            std::cout << "  --output <path>    Save trained model to file (default: trained_model.bin)\n";
            std::cout << "  --generate <n>      Generate n games for training\n";
            std::cout << "  --help             Show this help message\n";
            return 0;
        }
    }
    
    // Create or load NNUE network
    nnue::NNUE* network = new nnue::NNUE();
    
    if (!modelPath.empty()) {
        std::cout << "Loading model from " << modelPath << "...\n";
        if (!network->loadModel(modelPath)) {
            std::cerr << "Error: Failed to load model from " << modelPath << "\n";
            delete network;
            return 1;
        }
        std::cout << "Model loaded successfully.\n\n";
    } else {
        std::cout << "Using randomly initialized model.\n\n";
    }
    
    // Load or generate training data
    rl::TrainingDataset dataset;
    
    if (generateData) {
        std::cout << "Generating " << numGames << " games for training...\n";
        
        rl::SelfPlayConfig config;
        config.searchDepth = 3;
        config.maxMoves = 100;
        config.nnueModelPath = modelPath;
        
        rl::SelfPlayGenerator generator(config);
        auto games = generator.generateGames(numGames);
        
        for (const auto& game : games) {
            dataset.addGame(game);
        }
        
        std::cout << "Generated " << dataset.getGameCount() << " games with "
                 << dataset.getPositionCount() << " positions.\n\n";
        
        // Save dataset
        std::string autoDatasetPath = "training_data.bin";
        if (dataset.save(autoDatasetPath)) {
            std::cout << "Saved training data to " << autoDatasetPath << "\n\n";
        }
    } else if (!datasetPath.empty()) {
        std::cout << "Loading training data from " << datasetPath << "...\n";
        if (!dataset.load(datasetPath)) {
            std::cerr << "Error: Failed to load dataset from " << datasetPath << "\n";
            delete network;
            return 1;
        }
        std::cout << "Loaded " << dataset.getGameCount() << " games with "
                 << dataset.getPositionCount() << " positions.\n\n";
    } else {
        std::cerr << "Error: Must specify either --dataset or --generate\n";
        std::cerr << "Use --help for usage information.\n";
        delete network;
        return 1;
    }
    
    if (dataset.getPositionCount() == 0) {
        std::cerr << "Error: No training data available.\n";
        delete network;
        return 1;
    }
    
    // Configure training
    rl::TrainingConfig trainConfig;
    trainConfig.learningRate = 0.001f;
    trainConfig.batchSize = 256;
    trainConfig.numEpochs = 10;
    trainConfig.validationSplit = 0.1f;
    trainConfig.useAdam = true;
    trainConfig.saveInterval = 5;
    trainConfig.savePath = outputModelPath;
    
    // Create trainer
    rl::NNUETrainer trainer(network, trainConfig);
    
    // Train
    std::cout << "Starting training...\n\n";
    trainer.train(dataset);
    
    std::cout << "\n=== Training Complete ===\n";
    
    delete network;
    return 0;
}
