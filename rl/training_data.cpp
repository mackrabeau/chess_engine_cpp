#include "training_data.h"
#include <fstream>
#include <algorithm>
#include <random>
#include <cstring>
#include <iostream>

namespace rl {

TrainingDataset::TrainingDataset() {
}

TrainingDataset::~TrainingDataset() {
}

void TrainingDataset::addGame(const TrainingGame& game) {
    games.push_back(game);
    updatePositions();
}

void TrainingDataset::updatePositions() {
    allPositions.clear();
    for (const auto& game : games) {
        for (const auto& pos : game.positions) {
            allPositions.push_back(pos);
        }
    }
}

void TrainingDataset::clear() {
    games.clear();
    allPositions.clear();
}

void TrainingDataset::shuffle() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(allPositions.begin(), allPositions.end(), gen);
}

bool TrainingDataset::save(const std::string& path) const {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file for writing: " << path << std::endl;
        return false;
    }
    
    // Magic number: "TRDT" (Training Data)
    const char magic[4] = {'T', 'R', 'D', 'T'};
    file.write(magic, 4);
    if (!file.good()) return false;
    
    // Version number
    const uint32_t version = 1;
    file.write(reinterpret_cast<const char*>(&version), sizeof(uint32_t));
    if (!file.good()) return false;
    
    // Number of games
    uint32_t numGames = static_cast<uint32_t>(games.size());
    file.write(reinterpret_cast<const char*>(&numGames), sizeof(uint32_t));
    if (!file.good()) return false;
    
    // Write each game
    for (const auto& game : games) {
        // Game metadata
        uint32_t numPositions = static_cast<uint32_t>(game.positions.size());
        file.write(reinterpret_cast<const char*>(&numPositions), sizeof(uint32_t));
        if (!file.good()) return false;
        
        // Result string length and data
        uint32_t resultLen = static_cast<uint32_t>(game.result.size());
        file.write(reinterpret_cast<const char*>(&resultLen), sizeof(uint32_t));
        file.write(game.result.c_str(), resultLen);
        if (!file.good()) return false;
        
        // Reason string length and data
        uint32_t reasonLen = static_cast<uint32_t>(game.reason.size());
        file.write(reinterpret_cast<const char*>(&reasonLen), sizeof(uint32_t));
        file.write(game.reason.c_str(), reasonLen);
        if (!file.good()) return false;
        
        // Starting FEN length and data
        uint32_t fenLen = static_cast<uint32_t>(game.startingFen.size());
        file.write(reinterpret_cast<const char*>(&fenLen), sizeof(uint32_t));
        file.write(game.startingFen.c_str(), fenLen);
        if (!file.good()) return false;
        
        // Write positions
        for (const auto& pos : game.positions) {
            // FEN
            uint32_t posFenLen = static_cast<uint32_t>(pos.fen.size());
            file.write(reinterpret_cast<const char*>(&posFenLen), sizeof(uint32_t));
            file.write(pos.fen.c_str(), posFenLen);
            if (!file.good()) return false;
            
            // Target evaluation
            file.write(reinterpret_cast<const char*>(&pos.targetEval), sizeof(float));
            if (!file.good()) return false;
            
            // Search depth
            file.write(reinterpret_cast<const char*>(&pos.searchDepth), sizeof(int));
            if (!file.good()) return false;
            
            // Move
            file.write(reinterpret_cast<const char*>(&pos.move), sizeof(Move));
            if (!file.good()) return false;
            
            // Move number
            file.write(reinterpret_cast<const char*>(&pos.moveNumber), sizeof(int));
            if (!file.good()) return false;
            
            // Is white
            file.write(reinterpret_cast<const char*>(&pos.isWhite), sizeof(bool));
            if (!file.good()) return false;
        }
    }
    
    file.close();
    return true;
}

bool TrainingDataset::load(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file for reading: " << path << std::endl;
        return false;
    }
    
    // Read and verify magic number
    char magic[4];
    file.read(magic, 4);
    if (!file.good() || magic[0] != 'T' || magic[1] != 'R' || magic[2] != 'D' || magic[3] != 'T') {
        std::cerr << "Error: Invalid magic number in training data file" << std::endl;
        return false;
    }
    
    // Read version
    uint32_t version;
    file.read(reinterpret_cast<char*>(&version), sizeof(uint32_t));
    if (!file.good()) return false;
    if (version != 1) {
        std::cerr << "Error: Unsupported training data version: " << version << std::endl;
        return false;
    }
    
    // Clear existing data
    clear();
    
    // Read number of games
    uint32_t numGames;
    file.read(reinterpret_cast<char*>(&numGames), sizeof(uint32_t));
    if (!file.good()) return false;
    
    // Read each game
    for (uint32_t g = 0; g < numGames; ++g) {
        TrainingGame game;
        
        // Number of positions
        uint32_t numPositions;
        file.read(reinterpret_cast<char*>(&numPositions), sizeof(uint32_t));
        if (!file.good()) return false;
        
        // Result
        uint32_t resultLen;
        file.read(reinterpret_cast<char*>(&resultLen), sizeof(uint32_t));
        if (!file.good()) return false;
        game.result.resize(resultLen);
        file.read(&game.result[0], resultLen);
        if (!file.good()) return false;
        
        // Reason
        uint32_t reasonLen;
        file.read(reinterpret_cast<char*>(&reasonLen), sizeof(uint32_t));
        if (!file.good()) return false;
        game.reason.resize(reasonLen);
        file.read(&game.reason[0], reasonLen);
        if (!file.good()) return false;
        
        // Starting FEN
        uint32_t fenLen;
        file.read(reinterpret_cast<char*>(&fenLen), sizeof(uint32_t));
        if (!file.good()) return false;
        game.startingFen.resize(fenLen);
        file.read(&game.startingFen[0], fenLen);
        if (!file.good()) return false;
        
        // Read positions
        for (uint32_t p = 0; p < numPositions; ++p) {
            TrainingPosition pos;
            
            // FEN
            uint32_t posFenLen;
            file.read(reinterpret_cast<char*>(&posFenLen), sizeof(uint32_t));
            if (!file.good()) return false;
            pos.fen.resize(posFenLen);
            file.read(&pos.fen[0], posFenLen);
            if (!file.good()) return false;
            
            // Target evaluation
            file.read(reinterpret_cast<char*>(&pos.targetEval), sizeof(float));
            if (!file.good()) return false;
            
            // Search depth
            file.read(reinterpret_cast<char*>(&pos.searchDepth), sizeof(int));
            if (!file.good()) return false;
            
            // Move
            file.read(reinterpret_cast<char*>(&pos.move), sizeof(Move));
            if (!file.good()) return false;
            
            // Move number
            file.read(reinterpret_cast<char*>(&pos.moveNumber), sizeof(int));
            if (!file.good()) return false;
            
            // Is white
            file.read(reinterpret_cast<char*>(&pos.isWhite), sizeof(bool));
            if (!file.good()) return false;
            
            game.positions.push_back(pos);
        }
        
        games.push_back(game);
    }
    
    updatePositions();
    file.close();
    return true;
}

} // namespace rl
