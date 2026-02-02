#include "nnue.h"
#include "board.h"
#include "move.h"
#include <algorithm>
#include <cstring>
#include <random>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>

namespace nnue {

int FeatureExtractor::getFeatureIndex(enumPiece pieceType, enumPiece pieceColor,
                                       int pieceSquare, int kingSquare, int perspective) {
    // Validate inputs
    if (pieceType == nKings || pieceType == nEmpty) {
        return -1; // Invalid feature
    }
    if (pieceSquare < 0 || pieceSquare >= 64 || kingSquare < 0 || kingSquare >= 64) {
        return -1; // Invalid square
    }
    if (perspective < 0 || perspective > 1) {
        return -1; // Invalid perspective
    }
    
    // Get piece type index (0-4: pawn, knight, bishop, rook, queen)
    int ptIndex = getPieceTypeIndex(pieceType);
    if (ptIndex < 0) return -1;
    
    // Color index: 0 for white, 1 for black
    int colorIndex = (pieceColor == nWhite) ? 0 : 1;
    
    // Feature index formula:
    // perspective_offset + (piece_type * 2 * 64 * 64) + (color * 64 * 64) + (piece_square * 64) + king_square
    int perspectiveOffset = perspective * FEATURES_PER_PERSPECTIVE;
    int featureIndex = perspectiveOffset + 
                       (ptIndex * 2 * 64 * 64) + 
                       (colorIndex * 64 * 64) + 
                       (pieceSquare * 64) + 
                       kingSquare;
    
    return featureIndex;
}

int FeatureExtractor::getKingSquare(const Board& board, int perspective) {
    if (perspective == 0) {
        // White perspective: use white king
        U64 whiteKing = board.getWhiteKing();
        if (whiteKing == 0) return -1; // No king (shouldn't happen in valid positions)
        return __builtin_ctzll(whiteKing);
    } else {
        // Black perspective: use black king
        U64 blackKing = board.getBlackKing();
        if (blackKing == 0) return -1; // No king (shouldn't happen in valid positions)
        return __builtin_ctzll(blackKing);
    }
}

std::vector<int> FeatureExtractor::extractFeatures(const Board& board, int perspective) {
    std::vector<int> features;
    features.reserve(32); // Typical position has ~16 pieces per side, reserve space
    
    // Get king square for this perspective
    int kingSquare = getKingSquare(board, perspective);
    if (kingSquare < 0) {
        return features; // No king found, return empty
    }
    
    // Iterate through all pieces (except kings)
    enumPiece pieceTypes[] = {nPawns, nKnights, nBishops, nRooks, nQueens};
    
    for (int ptIdx = 0; ptIdx < 5; ++ptIdx) {
        enumPiece pieceType = pieceTypes[ptIdx];
        
        // Check both colors
        for (int colorIdx = 0; colorIdx < 2; ++colorIdx) {
            enumPiece pieceColor = (colorIdx == 0) ? nWhite : nBlack;
            
            // Get bitboard for this piece type and color
            U64 pieces;
            if (pieceColor == nWhite) {
                switch(pieceType) {
                    case nPawns: pieces = board.getWhitePawns(); break;
                    case nKnights: pieces = board.getWhiteKnights(); break;
                    case nBishops: pieces = board.getWhiteBishops(); break;
                    case nRooks: pieces = board.getWhiteRooks(); break;
                    case nQueens: pieces = board.getWhiteQueens(); break;
                    default: pieces = 0; break;
                }
            } else {
                switch(pieceType) {
                    case nPawns: pieces = board.getBlackPawns(); break;
                    case nKnights: pieces = board.getBlackKnights(); break;
                    case nBishops: pieces = board.getBlackBishops(); break;
                    case nRooks: pieces = board.getBlackRooks(); break;
                    case nQueens: pieces = board.getBlackQueens(); break;
                    default: pieces = 0; break;
                }
            }
            
            // Extract features for each piece of this type
            U64 piecesCopy = pieces;
            while (piecesCopy) {
                int square = __builtin_ctzll(piecesCopy);
                piecesCopy &= piecesCopy - 1; // Clear least significant bit
                
                int featureIndex = getFeatureIndex(pieceType, pieceColor, square, kingSquare, perspective);
                if (featureIndex >= 0) {
                    features.push_back(featureIndex);
                }
            }
        }
    }
    
    return features;
}


Accumulator::Accumulator() {
    // Initialize hidden layer for both perspectives
    // For now, we'll just store feature indices. The actual hidden layer
    // will be computed by the NNUE network using weights.
    hidden1.resize(2);
    activeFeatures.resize(2);
    
    // Initialize with empty vectors
    for (int p = 0; p < 2; ++p) {
        hidden1[p].resize(256, 0.0f); // 256 neurons in first hidden layer
        activeFeatures[p].clear();
    }
    
    whiteKingSquare = -1;
    blackKingSquare = -1;
}

void Accumulator::reset() {
    for (int p = 0; p < 2; ++p) {
        hidden1[p].assign(256, 0.0f);
        activeFeatures[p].clear();
    }
    whiteKingSquare = -1;
    blackKingSquare = -1;
}

void Accumulator::refresh(const Board& board, const NNUE* network) {
    reset();
    
    // Extract features for both perspectives
    activeFeatures[0] = FeatureExtractor::extractFeatures(board, 0); // White perspective
    activeFeatures[1] = FeatureExtractor::extractFeatures(board, 1); // Black perspective
    
    // Store king squares
    whiteKingSquare = FeatureExtractor::getKingSquare(board, 0);
    blackKingSquare = FeatureExtractor::getKingSquare(board, 1);
    
    // If network is provided, compute hidden1 layer
    if (network) {
        network->updateAccumulatorHidden1(*this, board);
    } else {
        // Initialize hidden1 to biases only (will be computed later when network is available)
        // For now, just keep zeros
    }
}

void Accumulator::addFeatureToHidden1(int featureIndex, int perspective, const NNUE& network) {
    if (featureIndex < 0 || featureIndex >= NNUE::INPUT_SIZE) return;
    if (perspective < 0 || perspective > 1) return;
    
    // Get the weight vector for this feature (const version)
    const auto& weights = network.getInputWeights();
    if (featureIndex >= (int)weights.size()) return;
    
    // Add feature's weights to hidden1
    auto& hidden1Out = hidden1[perspective];
    const auto& featureWeights = weights[featureIndex];
    
    for (size_t i = 0; i < hidden1Out.size() && i < featureWeights.size(); ++i) {
        hidden1Out[i] += featureWeights[i];
    }
    
    // Apply ReLU (in case value was negative and became positive)
    for (size_t i = 0; i < hidden1Out.size(); ++i) {
        if (hidden1Out[i] < 0.0f) hidden1Out[i] = 0.0f;
    }
}

void Accumulator::removeFeatureFromHidden1(int featureIndex, int perspective, const NNUE& network) {
    if (featureIndex < 0 || featureIndex >= NNUE::INPUT_SIZE) return;
    if (perspective < 0 || perspective > 1) return;
    
    // Get the weight vector for this feature 
    const auto& weights = network.getInputWeights();
    if (featureIndex >= (int)weights.size()) return;
    
    // Remove feature's weights from hidden1
    auto& hidden1Out = hidden1[perspective];
    const auto& featureWeights = weights[featureIndex];
    
    for (size_t i = 0; i < hidden1Out.size() && i < featureWeights.size(); ++i) {
        hidden1Out[i] -= featureWeights[i];
    }
    
    // Apply ReLU (in case value became negative)
    for (size_t i = 0; i < hidden1Out.size(); ++i) {
        if (hidden1Out[i] < 0.0f) hidden1Out[i] = 0.0f;
    }
}

void Accumulator::update(const Board& boardBefore, const Board& boardAfter, Move move, const NNUE* network) {
    // For MVP, we'll do a simple refresh if king moved, otherwise incremental update
    // This is a simplified version - full incremental update will be optimized later
    
    int from = getFrom(move);
    int to = getTo(move);
    
    // Get piece type from the "from" square in boardBefore (before the move)
    // This is the piece that's moving
    enumPiece originalPieceType = boardBefore.getPieceType(from);
    if (originalPieceType == nEmpty) {
        // Invalid move - square is empty, refresh instead
        refresh(boardAfter, network);
        return;
    }
    enumPiece pieceColor = boardBefore.getColourType(from);
    
    // Check if king moved
    bool whiteKingMoved = (originalPieceType == nKings && pieceColor == nWhite);
    bool blackKingMoved = (originalPieceType == nKings && pieceColor == nBlack);
    
    if (whiteKingMoved || blackKingMoved) {
        // King moved - need to refresh all features for that perspective
        // For MVP, refresh both perspectives (optimization: only refresh affected perspective)
        refresh(boardAfter, network);
        return;
    }
    
    // For non-king moves, we can do incremental update
    // Remove old feature, add new feature
    
    // Get old and new king squares
    int oldWhiteKing = FeatureExtractor::getKingSquare(boardBefore, 0);
    int newWhiteKing = FeatureExtractor::getKingSquare(boardAfter, 0);
    int oldBlackKing = FeatureExtractor::getKingSquare(boardBefore, 1);
    int newBlackKing = FeatureExtractor::getKingSquare(boardAfter, 1);
    
    // Update king squares
    whiteKingSquare = newWhiteKing;
    blackKingSquare = newBlackKing;
    
    // If king square changed, refresh (shouldn't happen for non-king moves, but check anyway)
    if (oldWhiteKing != newWhiteKing || oldBlackKing != newBlackKing) {
        refresh(boardAfter, network);
        return;
    }
    
    // Incremental update: remove features from 'from' square, add features to 'to' square
    // Also handle captures
    
    // Get captured piece info
    enumPiece capturedPiece = getCapturedPiece(move);
    bool isCapture = (capturedPiece != nEmpty);
    
    // Update features for both perspectives
    for (int perspective = 0; perspective < 2; ++perspective) {
        int kingSquare = (perspective == 0) ? whiteKingSquare : blackKingSquare;
        
        // Remove old feature from 'from' square
        // Use original piece type (before move)
        if (originalPieceType != nKings && originalPieceType != nEmpty) {
            int oldFeature = FeatureExtractor::getFeatureIndex(
                originalPieceType, pieceColor, from, kingSquare, perspective);
            if (oldFeature >= 0) {
                auto it = std::find(activeFeatures[perspective].begin(), 
                                   activeFeatures[perspective].end(), oldFeature);
                if (it != activeFeatures[perspective].end()) {
                    activeFeatures[perspective].erase(it);
                    // Update hidden1 incrementally if network is provided
                    if (network) {
                        removeFeatureFromHidden1(oldFeature, perspective, *network);
                    }
                }
            }
        }
        
        // Remove captured piece feature
        if (isCapture && capturedPiece != nKings && capturedPiece != nEmpty) {
            enumPiece capturedColor = (pieceColor == nWhite) ? nBlack : nWhite;
            int capturedFeature = FeatureExtractor::getFeatureIndex(
                capturedPiece, capturedColor, to, kingSquare, perspective);
            if (capturedFeature >= 0) {
                auto it = std::find(activeFeatures[perspective].begin(), 
                                   activeFeatures[perspective].end(), capturedFeature);
                if (it != activeFeatures[perspective].end()) {
                    activeFeatures[perspective].erase(it);
                    // Update hidden1 incrementally if network is provided
                    if (network) {
                        removeFeatureFromHidden1(capturedFeature, perspective, *network);
                    }
                }
            }
        }
        
        // Add new feature to 'to' square
        // Handle promotion - get final piece type from boardAfter
        enumPiece finalPieceType = originalPieceType; // Default to original piece
        if (isPromotion(move)) {
            // Determine promotion piece type from move flags
            int flags = getFlags(move);
            if (flags == KNIGHT_PROMO || flags == KNIGHT_PROMO_CAPTURE) {
                finalPieceType = nKnights;
            } else if (flags == BISHOP_PROMO || flags == BISHOP_PROMO_CAPTURE) {
                finalPieceType = nBishops;
            } else if (flags == ROOK_PROMO || flags == ROOK_PROMO_CAPTURE) {
                finalPieceType = nRooks;
            } else if (flags == QUEEN_PROMO || flags == QUEEN_PROMO_CAPTURE) {
                finalPieceType = nQueens;
            }
        } else {
            // For non-promotions, piece type stays the same
            // (originalPieceType is already set correctly)
        }
        
        if (finalPieceType != nKings && finalPieceType != nEmpty) {
            int newFeature = FeatureExtractor::getFeatureIndex(
                finalPieceType, pieceColor, to, kingSquare, perspective);
            if (newFeature >= 0) {
                // Check if not already present (shouldn't happen, but be safe)
                auto it = std::find(activeFeatures[perspective].begin(), 
                                   activeFeatures[perspective].end(), newFeature);
                if (it == activeFeatures[perspective].end()) {
                    activeFeatures[perspective].push_back(newFeature);
                    // Update hidden1 incrementally if network is provided
                    if (network) {
                        addFeatureToHidden1(newFeature, perspective, *network);
                    }
                }
            }
        }
    }
}

const std::vector<int>& Accumulator::getActiveFeatures(int perspective) const {
    if (perspective < 0 || perspective > 1) {
        static std::vector<int> empty;
        return empty;
    }
    return activeFeatures[perspective];
}

const std::vector<float>& Accumulator::getHidden1(int perspective) const {
    if (perspective < 0 || perspective > 1) {
        static std::vector<float> empty;
        return empty;
    }
    return hidden1[perspective];
}

std::vector<float>& Accumulator::getHidden1Mutable(int perspective) {
    if (perspective < 0 || perspective > 1) {
        static std::vector<float> empty;
        return empty;
    }
    return hidden1[perspective];
}

// ============================================================================
// NNUE Network Implementation
// ============================================================================

NNUE::NNUE() : outputBias(0.0f) {
    // Initialize weight vectors
    inputWeights.resize(INPUT_SIZE);
    for (int i = 0; i < INPUT_SIZE; ++i) {
        inputWeights[i].resize(HIDDEN1_SIZE, 0.0f);
    }
    inputBiases.resize(HIDDEN1_SIZE, 0.0f);
    
    hidden1Weights.resize(HIDDEN1_SIZE);
    for (int i = 0; i < HIDDEN1_SIZE; ++i) {
        hidden1Weights[i].resize(HIDDEN2_SIZE, 0.0f);
    }
    hidden1Biases.resize(HIDDEN2_SIZE, 0.0f);
    
    outputWeights.resize(HIDDEN2_SIZE, 0.0f);
    
    // Initialize with random weights
    initialize();
}

NNUE::~NNUE() {
    // Nothing to clean up (vectors handle their own memory)
}

void NNUE::initialize() {
    // Xavier/Glorot initialization for better gradient flow
    // For a layer with n inputs and m outputs:
    // weights ~ Uniform(-sqrt(6/(n+m)), sqrt(6/(n+m)))
    
    std::random_device rd;
    std::mt19937 gen(rd());
    
    // Initialize input -> hidden1 weights
    // n = 1 (each feature is binary), m = HIDDEN1_SIZE
    float inputScale = std::sqrt(6.0f / (1.0f + HIDDEN1_SIZE));
    std::uniform_real_distribution<float> inputDist(-inputScale, inputScale);
    
    for (int i = 0; i < INPUT_SIZE; ++i) {
        for (int j = 0; j < HIDDEN1_SIZE; ++j) {
            inputWeights[i][j] = inputDist(gen);
        }
    }
    // Biases initialized to zero
    std::fill(inputBiases.begin(), inputBiases.end(), 0.0f);
    
    // Initialize hidden1 -> hidden2 weights
    float hidden1Scale = std::sqrt(6.0f / (HIDDEN1_SIZE + HIDDEN2_SIZE));
    std::uniform_real_distribution<float> hidden1Dist(-hidden1Scale, hidden1Scale);
    
    for (int i = 0; i < HIDDEN1_SIZE; ++i) {
        for (int j = 0; j < HIDDEN2_SIZE; ++j) {
            hidden1Weights[i][j] = hidden1Dist(gen);
        }
    }
    std::fill(hidden1Biases.begin(), hidden1Biases.end(), 0.0f);
    
    // Initialize hidden2 -> output weights
    float outputScale = std::sqrt(6.0f / (HIDDEN2_SIZE + OUTPUT_SIZE));
    std::uniform_real_distribution<float> outputDist(-outputScale, outputScale);
    
    for (int i = 0; i < HIDDEN2_SIZE; ++i) {
        outputWeights[i] = outputDist(gen);
    }
    outputBias = 0.0f;
}

float NNUE::relu(float x) {
    return (x > 0.0f) ? x : 0.0f;
}

void NNUE::computeHidden1(const std::vector<int>& activeFeatures, int perspective,
                          std::vector<float>& hidden1Out) {
    // Initialize hidden1 to biases
    for (int i = 0; i < HIDDEN1_SIZE; ++i) {
        hidden1Out[i] = inputBiases[i];
    }
    
    // Add contributions from active features
    // Each active feature contributes its weight vector
    int perspectiveOffset = perspective * FeatureExtractor::FEATURES_PER_PERSPECTIVE;
    
    for (int featureIdx : activeFeatures) {
        // Adjust feature index to be relative to perspective
        int adjustedIdx = featureIdx - perspectiveOffset;
        
        // Validate index
        if (adjustedIdx >= 0 && adjustedIdx < FeatureExtractor::FEATURES_PER_PERSPECTIVE) {
            // Add weights for this feature to hidden1
            int fullFeatureIdx = featureIdx; // Use full index for weight lookup
            if (fullFeatureIdx >= 0 && fullFeatureIdx < INPUT_SIZE) {
                for (int j = 0; j < HIDDEN1_SIZE; ++j) {
                    hidden1Out[j] += inputWeights[fullFeatureIdx][j];
                }
            }
        }
    }
    
    // Apply ReLU activation
    for (int i = 0; i < HIDDEN1_SIZE; ++i) {
        hidden1Out[i] = relu(hidden1Out[i]);
    }
}

void NNUE::computeHidden2(const std::vector<float>& hidden1, std::vector<float>& hidden2Out) {
    // Initialize hidden2 to biases
    for (int i = 0; i < HIDDEN2_SIZE; ++i) {
        hidden2Out[i] = hidden1Biases[i];
    }
    
    // Matrix multiplication: hidden2 = hidden1 * hidden1Weights
    for (int i = 0; i < HIDDEN1_SIZE; ++i) {
        for (int j = 0; j < HIDDEN2_SIZE; ++j) {
            hidden2Out[j] += hidden1[i] * hidden1Weights[i][j];
        }
    }
    
    // Apply ReLU activation
    for (int i = 0; i < HIDDEN2_SIZE; ++i) {
        hidden2Out[i] = relu(hidden2Out[i]);
    }
}

float NNUE::computeOutput(const std::vector<float>& hidden2) {
    float output = outputBias;
    
    // Dot product: output = hidden2 * outputWeights
    for (int i = 0; i < HIDDEN2_SIZE; ++i) {
        output += hidden2[i] * outputWeights[i];
    }
    
    // No activation function for output (linear)
    return output;
}

float NNUE::evaluate(const Accumulator& accumulator) {
    // Compute hidden1 for both perspectives
    std::vector<float> whiteHidden1(HIDDEN1_SIZE);
    std::vector<float> blackHidden1(HIDDEN1_SIZE);
    
    computeHidden1(accumulator.getActiveFeatures(0), 0, whiteHidden1);
    computeHidden1(accumulator.getActiveFeatures(1), 1, blackHidden1);
    
    // Combine perspectives: subtract black from white
    // This gives evaluation from white's perspective
    std::vector<float> combinedHidden1(HIDDEN1_SIZE);
    for (int i = 0; i < HIDDEN1_SIZE; ++i) {
        combinedHidden1[i] = whiteHidden1[i] - blackHidden1[i];
    }
    
    // Compute hidden2
    std::vector<float> hidden2(HIDDEN2_SIZE);
    computeHidden2(combinedHidden1, hidden2);
    
    // Compute output
    float evaluation = computeOutput(hidden2);
    
    return evaluation;
}

float NNUE::evaluate(const Board& board) {
    // Create accumulator and refresh from board
    Accumulator acc;
    acc.refresh(board);
    
    // Evaluate using accumulator
    return evaluate(acc);
}

void NNUE::updateAccumulatorHidden1(Accumulator& accumulator, const Board& /* board */) const {
    // Compute hidden1 for both perspectives and store in accumulator
    // Note: computeHidden1 needs to be const or we need a const version
    // For now, we'll compute directly here
    const int HIDDEN1_SIZE = 256;
    
    // Initialize hidden1 to biases
    for (int p = 0; p < 2; ++p) {
        auto& hidden1Out = accumulator.getHidden1Mutable(p);
        for (int i = 0; i < HIDDEN1_SIZE; ++i) {
            hidden1Out[i] = inputBiases[i];
        }
        
        // Add contributions from active features
        const auto& features = accumulator.getActiveFeatures(p);
        
        for (int featureIdx : features) {
            if (featureIdx >= 0 && featureIdx < INPUT_SIZE) {
                for (int j = 0; j < HIDDEN1_SIZE; ++j) {
                    hidden1Out[j] += inputWeights[featureIdx][j];
                }
            }
        }
        
        // Apply ReLU
        for (int i = 0; i < HIDDEN1_SIZE; ++i) {
            if (hidden1Out[i] < 0.0f) hidden1Out[i] = 0.0f;
        }
    }
}

bool NNUE::saveModel(const std::string& path) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file for writing: " << path << std::endl;
        return false;
    }
    
    // Magic number: "NNUE"
    const char magic[4] = {'N', 'N', 'U', 'E'};
    file.write(magic, 4);
    if (!file.good()) return false;
    
    // Version number (uint32_t)
    const uint32_t version = 1;
    file.write(reinterpret_cast<const char*>(&version), sizeof(uint32_t));
    if (!file.good()) return false;
    
    // Architecture info
    uint32_t arch[4] = {
        static_cast<uint32_t>(INPUT_SIZE),
        static_cast<uint32_t>(HIDDEN1_SIZE),
        static_cast<uint32_t>(HIDDEN2_SIZE),
        static_cast<uint32_t>(OUTPUT_SIZE)
    };
    file.write(reinterpret_cast<const char*>(arch), 4 * sizeof(uint32_t));
    if (!file.good()) return false;
    
    // Write input weights [INPUT_SIZE][HIDDEN1_SIZE]
    for (int i = 0; i < INPUT_SIZE; ++i) {
        file.write(reinterpret_cast<const char*>(inputWeights[i].data()), 
                   HIDDEN1_SIZE * sizeof(float));
        if (!file.good()) return false;
    }
    
    // Write input biases [HIDDEN1_SIZE]
    file.write(reinterpret_cast<const char*>(inputBiases.data()), 
               HIDDEN1_SIZE * sizeof(float));
    if (!file.good()) return false;
    
    // Write hidden1 weights [HIDDEN1_SIZE][HIDDEN2_SIZE]
    for (int i = 0; i < HIDDEN1_SIZE; ++i) {
        file.write(reinterpret_cast<const char*>(hidden1Weights[i].data()), 
                   HIDDEN2_SIZE * sizeof(float));
        if (!file.good()) return false;
    }
    
    // Write hidden1 biases [HIDDEN2_SIZE]
    file.write(reinterpret_cast<const char*>(hidden1Biases.data()), 
               HIDDEN2_SIZE * sizeof(float));
    if (!file.good()) return false;
    
    // Write output weights [HIDDEN2_SIZE]
    file.write(reinterpret_cast<const char*>(outputWeights.data()), 
               HIDDEN2_SIZE * sizeof(float));
    if (!file.good()) return false;
    
    // Write output bias
    file.write(reinterpret_cast<const char*>(&outputBias), sizeof(float));
    if (!file.good()) return false;
    
    file.close();
    return true;
}

bool NNUE::loadModel(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file for reading: " << path << std::endl;
        return false;
    }
    
    // Read and verify magic number
    char magic[4];
    file.read(magic, 4);
    if (!file.good() || magic[0] != 'N' || magic[1] != 'N' || magic[2] != 'U' || magic[3] != 'E') {
        std::cerr << "Error: Invalid magic number in model file" << std::endl;
        return false;
    }
    
    // Read version
    uint32_t version;
    file.read(reinterpret_cast<char*>(&version), sizeof(uint32_t));
    if (!file.good()) return false;
    if (version != 1) {
        std::cerr << "Error: Unsupported model version: " << version << std::endl;
        return false;
    }
    
    // Read architecture info
    uint32_t arch[4];
    file.read(reinterpret_cast<char*>(arch), 4 * sizeof(uint32_t));
    if (!file.good()) return false;
    
    // Verify architecture matches
    if (arch[0] != static_cast<uint32_t>(INPUT_SIZE) ||
        arch[1] != static_cast<uint32_t>(HIDDEN1_SIZE) ||
        arch[2] != static_cast<uint32_t>(HIDDEN2_SIZE) ||
        arch[3] != static_cast<uint32_t>(OUTPUT_SIZE)) {
        std::cerr << "Error: Model architecture mismatch. Expected: [" 
                  << INPUT_SIZE << ", " << HIDDEN1_SIZE << ", " 
                  << HIDDEN2_SIZE << ", " << OUTPUT_SIZE << "], Got: ["
                  << arch[0] << ", " << arch[1] << ", " << arch[2] << ", " << arch[3] << "]" << std::endl;
        return false;
    }
    
    // Read input weights [INPUT_SIZE][HIDDEN1_SIZE]
    for (int i = 0; i < INPUT_SIZE; ++i) {
        file.read(reinterpret_cast<char*>(inputWeights[i].data()), 
                  HIDDEN1_SIZE * sizeof(float));
        if (!file.good()) return false;
    }
    
    // Read input biases [HIDDEN1_SIZE]
    file.read(reinterpret_cast<char*>(inputBiases.data()), 
              HIDDEN1_SIZE * sizeof(float));
    if (!file.good()) return false;
    
    // Read hidden1 weights [HIDDEN1_SIZE][HIDDEN2_SIZE]
    for (int i = 0; i < HIDDEN1_SIZE; ++i) {
        file.read(reinterpret_cast<char*>(hidden1Weights[i].data()), 
                  HIDDEN2_SIZE * sizeof(float));
        if (!file.good()) return false;
    }
    
    // Read hidden1 biases [HIDDEN2_SIZE]
    file.read(reinterpret_cast<char*>(hidden1Biases.data()), 
              HIDDEN2_SIZE * sizeof(float));
    if (!file.good()) return false;
    
    // Read output weights [HIDDEN2_SIZE]
    file.read(reinterpret_cast<char*>(outputWeights.data()), 
              HIDDEN2_SIZE * sizeof(float));
    if (!file.good()) return false;
    
    // Read output bias
    file.read(reinterpret_cast<char*>(&outputBias), sizeof(float));
    if (!file.good()) return false;
    
    file.close();
    return true;
}

} // namespace nnue
