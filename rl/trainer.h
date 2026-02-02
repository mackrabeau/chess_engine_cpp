#ifndef TRAINER_H
#define TRAINER_H

#include "../nnue.h"
#include "training_data.h"
#include <vector>
#include <string>
#include <memory>

namespace rl {

// Training configuration
struct TrainingConfig {
    float learningRate = 0.001f;      // Learning rate for SGD/Adam
    int batchSize = 256;              // Batch size for training
    int numEpochs = 10;               // Number of epochs
    float validationSplit = 0.1f;    // Fraction of data to use for validation
    bool useAdam = true;              // Use Adam optimizer (false = SGD)
    float adamBeta1 = 0.9f;           // Adam beta1 parameter
    float adamBeta2 = 0.999f;        // Adam beta2 parameter
    float adamEpsilon = 1e-8f;        // Adam epsilon parameter
    int saveInterval = 10;            // Save model every N epochs (0 = never)
    std::string savePath = "";        // Path to save model
    
    TrainingConfig() {}
};

// Training statistics
struct TrainingStats {
    float trainLoss = 0.0f;
    float valLoss = 0.0f;
    int epoch = 0;
    int batch = 0;
    int totalBatches = 0;
    
    TrainingStats() {}
};

// NNUE Trainer class
class NNUETrainer {
public:
    NNUETrainer(nnue::NNUE* network, const TrainingConfig& config);
    
    // Train on a dataset
    void train(TrainingDataset& dataset);
    
    // Train on a single batch
    void trainBatch(const std::vector<TrainingPosition>& batch);
    
    // Get current training statistics
    const TrainingStats& getStats() const { return stats; }
    
    // Reset optimizer state (for new training session)
    void resetOptimizer();
    
private:
    nnue::NNUE* network;
    TrainingConfig config;
    TrainingStats stats;
    
    // Adam optimizer state (momentum and variance estimates)
    std::vector<std::vector<float>> inputWeightsM;      // First moment
    std::vector<std::vector<float>> inputWeightsV;       // Second moment
    std::vector<float> inputBiasesM;
    std::vector<float> inputBiasesV;
    std::vector<std::vector<float>> hidden1WeightsM;
    std::vector<std::vector<float>> hidden1WeightsV;
    std::vector<float> hidden1BiasesM;
    std::vector<float> hidden1BiasesV;
    std::vector<float> outputWeightsM;
    std::vector<float> outputWeightsV;
    float outputBiasM = 0.0f;
    float outputBiasV = 0.0f;
    
    int optimizerStep = 0;  // Step counter for Adam
    
    // Initialize optimizer state
    void initializeOptimizer();
    
    // Forward pass for a single position
    float forwardPass(const TrainingPosition& position, 
                     std::vector<float>& hidden1White,
                     std::vector<float>& hidden1Black,
                     std::vector<float>& hidden2);
    
    // Backward pass: compute gradients
    void backwardPass(const TrainingPosition& position,
                     float prediction,
                     float target,
                     const std::vector<float>& hidden1White,
                     const std::vector<float>& hidden1Black,
                     const std::vector<float>& hidden2,
                     std::vector<std::vector<float>>& inputWeightsGrad,
                     std::vector<float>& inputBiasesGrad,
                     std::vector<std::vector<float>>& hidden1WeightsGrad,
                     std::vector<float>& hidden1BiasesGrad,
                     std::vector<float>& outputWeightsGrad,
                     float& outputBiasGrad);
    
    // Update weights using SGD or Adam
    void updateWeights(const std::vector<std::vector<float>>& inputWeightsGrad,
                      const std::vector<float>& inputBiasesGrad,
                      const std::vector<std::vector<float>>& hidden1WeightsGrad,
                      const std::vector<float>& hidden1BiasesGrad,
                      const std::vector<float>& outputWeightsGrad,
                      float outputBiasGrad,
                      int batchSize);
    
    // Compute MSE loss
    float computeLoss(float prediction, float target);
    
    // Split dataset into train/validation
    void splitDataset(const TrainingDataset& dataset,
                     std::vector<TrainingPosition>& train,
                     std::vector<TrainingPosition>& val);
};

} // namespace rl

#endif // TRAINER_H
