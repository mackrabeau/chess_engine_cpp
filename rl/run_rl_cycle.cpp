#include "rl_cycle.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    std::cout << "=== RL Training Cycle Program ===\n\n";
    
    rl::RLCycleConfig config;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--games" && i + 1 < argc) {
            config.selfPlayGames = std::stoi(argv[++i]);
        } else if (arg == "--epochs" && i + 1 < argc) {
            config.trainingEpochs = std::stoi(argv[++i]);
        } else if (arg == "--cycles" && i + 1 < argc) {
            config.maxCycles = std::stoi(argv[++i]);
        } else if (arg == "--eval-games" && i + 1 < argc) {
            config.evalGames = std::stoi(argv[++i]);
        } else if (arg == "--model" && i + 1 < argc) {
            config.modelPath = argv[++i];
        } else if (arg == "--threshold" && i + 1 < argc) {
            config.improvementThreshold = std::stof(argv[++i]);
        } else if (arg == "--help") {
            std::cout << "Usage: run_rl_cycle [options]\n";
            std::cout << "Options:\n";
            std::cout << "  --games <n>        Number of self-play games per cycle (default: 100)\n";
            std::cout << "  --epochs <n>       Training epochs per cycle (default: 10)\n";
            std::cout << "  --cycles <n>       Maximum number of cycles (default: 10, 0 = infinite)\n";
            std::cout << "  --eval-games <n>   Number of evaluation games (default: 20)\n";
            std::cout << "  --model <path>     Path to model file (default: rl_model.bin)\n";
            std::cout << "  --threshold <f>    Win rate threshold to accept model (default: 0.55)\n";
            std::cout << "  --help             Show this help message\n";
            return 0;
        }
    }
    
    // Create and run RL cycle
    rl::RLCycle cycle(config);
    cycle.runCycles();
    
    return 0;
}
