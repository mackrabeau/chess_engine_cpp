#ifndef NNUE_H
#define NNUE_H

#include "board.h"
#include "types.h"
#include <vector>
#include <cstdint>

// HalfKP feature set for NNUE evaluation
// Features: (piece_type, piece_color, piece_square, king_square)
// - 5 piece types (pawn, knight, bishop, rook, queen) - excluding kings
// - 2 colors (white, black)
// - 64 squares for piece
// - 64 squares for king
// Total: 5 * 2 * 64 * 64 = 40,960 features per perspective

namespace nnue {

// Forward declaration
class NNUE;

// Feature accumulator for efficient incremental updates
class Accumulator {
public:
    // Hidden layer activations for both perspectives
    // [perspective][neuron] where perspective: 0=white, 1=black
    std::vector<std::vector<float>> hidden1;  // First hidden layer (256 neurons)
    
    // Active feature indices for current position
    // [perspective][feature_index]
    std::vector<std::vector<int>> activeFeatures;
    
    // King squares for each perspective
    int whiteKingSquare;
    int blackKingSquare;
    
    Accumulator();
    void reset();
    
    // Initialize accumulator from board position
    // If network is provided, also computes hidden1 layer
    void refresh(const Board& board, const NNUE* network = nullptr);
    
    // Update accumulator incrementally for a move
    // If network is provided, also updates hidden1 layer incrementally
    void update(const Board& boardBefore, const Board& boardAfter, Move move, const NNUE* network = nullptr);
    
    // Get active feature indices for a perspective
    const std::vector<int>& getActiveFeatures(int perspective) const;
    
    // Get hidden layer activations for a perspective
    const std::vector<float>& getHidden1(int perspective) const;
    
    // Non-const access for updating (used by NNUE network)
    std::vector<float>& getHidden1Mutable(int perspective);
    
private:
    // Helper methods for incremental hidden1 updates
    // Add a feature's weight contribution to hidden1
    void addFeatureToHidden1(int featureIndex, int perspective, const NNUE& network);
    
    // Remove a feature's weight contribution from hidden1
    void removeFeatureFromHidden1(int featureIndex, int perspective, const NNUE& network);
};

// Feature extraction functions
class FeatureExtractor {
public:
    // Get feature index for HalfKP feature
    // pieceType: nPawns, nKnights, nBishops, nRooks, nQueens (not nKings)
    // pieceColor: nWhite or nBlack
    // pieceSquare: 0-63
    // kingSquare: 0-63 (king square of the side to move)
    // perspective: 0 for white perspective, 1 for black perspective
    static int getFeatureIndex(enumPiece pieceType, enumPiece pieceColor, 
                               int pieceSquare, int kingSquare, int perspective);
    
    // Extract all active features from a board position
    // Returns vector of feature indices that are active (1) in this position
    static std::vector<int> extractFeatures(const Board& board, int perspective);
    
    // Get king square for a given perspective
    // perspective 0 = white's perspective (white king)
    // perspective 1 = black's perspective (black king)
    static int getKingSquare(const Board& board, int perspective);
    
    // Total number of features per perspective
    static constexpr int FEATURES_PER_PERSPECTIVE = 5 * 2 * 64 * 64; // 40,960
    static constexpr int TOTAL_FEATURES = FEATURES_PER_PERSPECTIVE * 2; // 81,920
};

// Helper function to get piece type index (0-4 for pawn, knight, bishop, rook, queen)
inline int getPieceTypeIndex(enumPiece pieceType) {
    switch(pieceType) {
        case nPawns: return 0;
        case nKnights: return 1;
        case nBishops: return 2;
        case nRooks: return 3;
        case nQueens: return 4;
        default: return -1; // Invalid (kings are not included in HalfKP)
    }
}

// NNUE Neural Network Architecture
// Input (sparse) -> Hidden1 (256) -> Hidden2 (32) -> Output (1)
class NNUE {
public:
    // Network architecture constants
    static constexpr int INPUT_SIZE = FeatureExtractor::TOTAL_FEATURES; // 81,920
    static constexpr int HIDDEN1_SIZE = 256;
    static constexpr int HIDDEN2_SIZE = 32;
    static constexpr int OUTPUT_SIZE = 1;
    
    NNUE();
    ~NNUE();
    
    // Initialize network with random weights (Xavier/Glorot initialization)
    void initialize();
    
    // Forward pass: evaluate position using accumulator
    // Returns evaluation in centipawns (positive = white is better)
    float evaluate(const Accumulator& accumulator);
    
    // Forward pass: evaluate position directly from board (slower, for testing)
    float evaluate(const Board& board);
    
    // Update accumulator's hidden1 layer with current network weights
    // This is called during refresh/update to keep accumulator in sync
    void updateAccumulatorHidden1(Accumulator& accumulator, const Board& board) const;
    
    // Weight access (for training later)
    std::vector<std::vector<float>>& getInputWeights() { return inputWeights; }
    const std::vector<std::vector<float>>& getInputWeights() const { return inputWeights; }
    std::vector<float>& getInputBiases() { return inputBiases; }
    const std::vector<float>& getInputBiases() const { return inputBiases; }
    std::vector<std::vector<float>>& getHidden1Weights() { return hidden1Weights; }
    const std::vector<std::vector<float>>& getHidden1Weights() const { return hidden1Weights; }
    std::vector<float>& getHidden1Biases() { return hidden1Biases; }
    const std::vector<float>& getHidden1Biases() const { return hidden1Biases; }
    std::vector<float>& getOutputWeights() { return outputWeights; }
    const std::vector<float>& getOutputWeights() const { return outputWeights; }
    float& getOutputBias() { return outputBias; }
    float getOutputBias() const { return outputBias; }
    
    // Save/load model (to be implemented in next phase)
    bool saveModel(const std::string& path);
    bool loadModel(const std::string& path);
    
private:
    // Network weights
    // inputWeights[feature_index][neuron] - weights from input features to hidden1
    std::vector<std::vector<float>> inputWeights;  // [INPUT_SIZE][HIDDEN1_SIZE]
    std::vector<float> inputBiases;                // [HIDDEN1_SIZE]
    
    // hidden1Weights[neuron1][neuron2] - weights from hidden1 to hidden2
    std::vector<std::vector<float>> hidden1Weights; // [HIDDEN1_SIZE][HIDDEN2_SIZE]
    std::vector<float> hidden1Biases;                // [HIDDEN2_SIZE]
    
    // outputWeights[neuron] - weights from hidden2 to output
    std::vector<float> outputWeights;               // [HIDDEN2_SIZE]
    float outputBias;                               // scalar
    
    // Helper functions
    float relu(float x);
    void computeHidden1(const std::vector<int>& activeFeatures, int perspective,
                       std::vector<float>& hidden1Out);
    void computeHidden2(const std::vector<float>& hidden1, std::vector<float>& hidden2Out);
    float computeOutput(const std::vector<float>& hidden2);
};

} // namespace nnue

#endif // NNUE_H
