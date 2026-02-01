#include "test_config.h"
#include "engine_interface.h"
#include "game_runner.h"
#include "game_recorder.h"
#include "../movetables.h"
#include <iostream>
#include <vector>
#include <memory>
#include <iomanip>
#include <chrono>

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " <engine1_path> <engine1_name> <engine2_path> <engine2_name> [options]\n";
    std::cout << "Options:\n";
    std::cout << "  --movetime <ms>     Time per move in milliseconds (default: 1000)\n";
    std::cout << "  --output <dir>      Output directory for CSV and PGN files (default: ./test_results)\n";
    std::cout << "  --num-games <n>     Number of games to play per position (default: 1)\n";
    std::cout << "  --swap-colors       Play each position with both colors (default: false)\n";
    std::cout << "  --positions <file>  File with FEN positions (one per line, default: use predefined)\n";
    std::cout << "\n";
    std::cout << "Example:\n";
    std::cout << "  " << programName << " ./engine \"MyEngine\" /usr/bin/stockfish \"Stockfish\" --movetime 1000 --num-games 10\n";
}

TestConfig parseArguments(int argc, char* argv[]) {
    TestConfig config;
    
    if (argc < 5) {
        printUsage(argv[0]);
        exit(1);
    }
    
    config.engine1Path = argv[1];
    config.engine1Name = argv[2];
    config.engine2Path = argv[3];
    config.engine2Name = argv[4];
    config.movetimeMs = 1000;
    config.outputDir = "./test_results";
    config.numGames = 1;
    config.swapColors = false;
    
    for (int i = 5; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--movetime" && i + 1 < argc) {
            config.movetimeMs = std::stoi(argv[++i]);
        } else if (arg == "--output" && i + 1 < argc) {
            config.outputDir = argv[++i];
        } else if (arg == "--num-games" && i + 1 < argc) {
            config.numGames = std::stoi(argv[++i]);
        } else if (arg == "--swap-colors") {
            config.swapColors = true;
        } else if (arg == "--positions" && i + 1 < argc) {
            // TODO: Load positions from file
            std::cerr << "Loading positions from file not yet implemented, using predefined positions\n";
        } else if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            exit(0);
        }
    }
    
    return config;
}

int main(int argc, char* argv[]) {
    // Initialize move tables
    MoveTables::instance().init();
    
    // Parse arguments
    TestConfig config = parseArguments(argc, argv);
    
    std::cout << "=== Chess Engine Testing Framework ===\n";
    std::cout << "Engine 1: " << config.engine1Name << " (" << config.engine1Path << ")\n";
    std::cout << "Engine 2: " << config.engine2Name << " (" << config.engine2Path << ")\n";
    std::cout << "Move time: " << config.movetimeMs << " ms\n";
    std::cout << "Output directory: " << config.outputDir << "\n";
    std::cout << "Games per position: " << config.numGames << "\n";
    std::cout << "Swap colors: " << (config.swapColors ? "Yes" : "No") << "\n";
    std::cout << "\n";
    
    // Get test positions
    std::vector<std::string> positions = TestPositions::getAllPositions();
    std::cout << "Using " << positions.size() << " predefined positions\n\n";
    
    // Initialize engines
    std::cout << "Initializing engines...\n";
    std::unique_ptr<EngineInterface> engine1 = std::make_unique<EngineInterface>(config.engine1Path, config.engine1Name);
    std::unique_ptr<EngineInterface> engine2 = std::make_unique<EngineInterface>(config.engine2Path, config.engine2Name);
    
    if (!engine1->initialize()) {
        std::cerr << "Failed to initialize engine 1: " << config.engine1Name << std::endl;
        return 1;
    }
    std::cout << "Engine 1 initialized: " << config.engine1Name << "\n";
    
    if (!engine2->initialize()) {
        std::cerr << "Failed to initialize engine 2: " << config.engine2Name << std::endl;
        return 1;
    }
    std::cout << "Engine 2 initialized: " << config.engine2Name << "\n\n";
    
    // Initialize recorder
    GameRecorder recorder(config.outputDir);
    
    // Calculate total games
    int totalGames = positions.size() * config.numGames;
    if (config.swapColors) {
        totalGames *= 2;
    }
    
    std::cout << "Starting test suite (" << totalGames << " games total)...\n\n";
    
    int gameId = 1;
    int gamesCompleted = 0;
    auto startTime = std::chrono::steady_clock::now();
    
    // Run games
    for (const auto& fen : positions) {
        for (int gameNum = 0; gameNum < config.numGames; ++gameNum) {
            // Play with engine1 as white, engine2 as black
            std::cout << "[" << gameId << "/" << totalGames << "] ";
            std::cout << "Game " << gameId << " - " << config.engine1Name << " (White) vs " 
                      << config.engine2Name << " (Black)\n";
            std::cout << "  FEN: " << fen << "\n";
            
            GameRunner runner(engine1.get(), engine2.get(), fen, config.engine1Name, config.engine2Name);
            GameResult result = runner.runGame(config.movetimeMs);
            
            recorder.recordGame(result, gameId);
            
            std::cout << "  Result: " << result.result << " (" << result.reason << ")\n";
            std::cout << "  Moves: " << result.moves.size() << "\n";
            std::cout << "  Max depth: " << result.maxDepth << ", Total nodes: " << result.totalNodes << "\n";
            std::cout << "\n";
            
            gameId++;
            gamesCompleted++;
            
            // If swap colors, play the reverse
            if (config.swapColors) {
                std::cout << "[" << gameId << "/" << totalGames << "] ";
                std::cout << "Game " << gameId << " - " << config.engine2Name << " (White) vs " 
                          << config.engine1Name << " (Black)\n";
                std::cout << "  FEN: " << fen << "\n";
                
                GameRunner runner2(engine2.get(), engine1.get(), fen, config.engine2Name, config.engine1Name);
                GameResult result2 = runner2.runGame(config.movetimeMs);
                
                recorder.recordGame(result2, gameId);
                
                std::cout << "  Result: " << result2.result << " (" << result2.reason << ")\n";
                std::cout << "  Moves: " << result2.moves.size() << "\n";
                std::cout << "  Max depth: " << result2.maxDepth << ", Total nodes: " << result2.totalNodes << "\n";
                std::cout << "\n";
                
                gameId++;
                gamesCompleted++;
            }
        }
    }
    
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime).count();
    
    std::cout << "=== Test Suite Complete ===\n";
    std::cout << "Games completed: " << gamesCompleted << "\n";
    std::cout << "Total time: " << duration << " seconds\n";
    std::cout << "Results saved to: " << config.outputDir << "\n";
    std::cout << "  - CSV: " << config.outputDir << "/games.csv\n";
    std::cout << "  - PGN: " << config.outputDir << "/games.pgn\n";
    
    recorder.close();
    
    // Cleanup engines
    engine1->quit();
    engine2->quit();
    
    return 0;
}
