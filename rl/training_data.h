#ifndef TRAINING_DATA_H
#define TRAINING_DATA_H

#include "../game.h"
#include "../move.h"
#include <string>
#include <vector>
#include <cstdint>

namespace rl {

// Represents a single training position
struct TrainingPosition {
    std::string fen;              // Position in FEN format
    float targetEval;             // Target evaluation in centipawns (from game outcome or search)
    int searchDepth;              // Search depth used to get evaluation
    Move move;                    // Move played from this position
    int moveNumber;               // Move number in the game
    bool isWhite;                 // True if white to move
    
    TrainingPosition() : targetEval(0.0f), searchDepth(0), move(MOVE_NONE), 
                         moveNumber(0), isWhite(true) {}
};

// Represents a complete game for training
struct TrainingGame {
    std::vector<TrainingPosition> positions;  // All positions from the game
    std::string result;                      // "1-0", "0-1", "1/2-1/2"
    std::string reason;                      // "checkmate", "stalemate", "draw", etc.
    std::string startingFen;                 // Starting position FEN
    
    TrainingGame() : result("*"), reason("ongoing") {}
};

// Training dataset manager
class TrainingDataset {
public:
    TrainingDataset();
    ~TrainingDataset();
    
    // Add a game to the dataset
    void addGame(const TrainingGame& game);
    
    // Get total number of positions
    size_t getPositionCount() const { return allPositions.size(); }
    
    // Get total number of games
    size_t getGameCount() const { return games.size(); }
    
    // Get all positions (for training)
    const std::vector<TrainingPosition>& getAllPositions() const { return allPositions; }
    
    // Get all games
    const std::vector<TrainingGame>& getAllGames() const { return games; }
    
    // Save dataset to binary file
    bool save(const std::string& path) const;
    
    // Load dataset from binary file
    bool load(const std::string& path);
    
    // Clear all data
    void clear();
    
    // Shuffle positions (for training)
    void shuffle();
    
private:
    std::vector<TrainingGame> games;
    std::vector<TrainingPosition> allPositions;  // Flattened positions from all games
    
    // Helper to flatten games into positions
    void updatePositions();
};

} // namespace rl

#endif // TRAINING_DATA_H
