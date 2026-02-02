#include "trainer.h"
#include "../game.h"
#include "../movetables.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <random>
#include <cmath>

namespace rl {

NNUETrainer::NNUETrainer(nnue::NNUE* net, const TrainingConfig& cfg) 
    : network(net), config(cfg) {
    initializeOptimizer();
}

void NNUETrainer::initializeOptimizer() {
    if (!config.useAdam) {
        // SGD doesn't need state
        return;
    }
    
    // Initialize Adam optimizer state
    const int INPUT_SIZE = nnue::NNUE::INPUT_SIZE;
    const int HIDDEN1_SIZE = nnue::NNUE::HIDDEN1_SIZE;
    const int HIDDEN2_SIZE = nnue::NNUE::HIDDEN2_SIZE;
    
    // Input layer
    inputWeightsM.resize(INPUT_SIZE);
    inputWeightsV.resize(INPUT_SIZE);
    for (int i = 0; i < INPUT_SIZE; ++i) {
        inputWeightsM[i].resize(HIDDEN1_SIZE, 0.0f);
        inputWeightsV[i].resize(HIDDEN1_SIZE, 0.0f);
    }
    inputBiasesM.resize(HIDDEN1_SIZE, 0.0f);
    inputBiasesV.resize(HIDDEN1_SIZE, 0.0f);
    
    // Hidden1 layer
    hidden1WeightsM.resize(HIDDEN1_SIZE);
    hidden1WeightsV.resize(HIDDEN1_SIZE);
    for (int i = 0; i < HIDDEN1_SIZE; ++i) {
        hidden1WeightsM[i].resize(HIDDEN2_SIZE, 0.0f);
        hidden1WeightsV[i].resize(HIDDEN2_SIZE, 0.0f);
    }
    hidden1BiasesM.resize(HIDDEN2_SIZE, 0.0f);
    hidden1BiasesV.resize(HIDDEN2_SIZE, 0.0f);
    
    // Output layer
    outputWeightsM.resize(HIDDEN2_SIZE, 0.0f);
    outputWeightsV.resize(HIDDEN2_SIZE, 0.0f);
    outputBiasM = 0.0f;
    outputBiasV = 0.0f;
    
    optimizerStep = 0;
}

void NNUETrainer::resetOptimizer() {
    initializeOptimizer();
}

float NNUETrainer::forwardPass(const TrainingPosition& position,
                               std::vector<float>& hidden1White,
                               std::vector<float>& hidden1Black,
                               std::vector<float>& hidden2) {
    // Parse FEN and extract features
    Game game;
    game.setPosition(position.fen);
    
    // Extract features for both perspectives
    auto featuresWhite = nnue::FeatureExtractor::extractFeatures(game.board, 0);
    auto featuresBlack = nnue::FeatureExtractor::extractFeatures(game.board, 1);
    
    // Compute hidden1 for white perspective
    hidden1White.resize(nnue::NNUE::HIDDEN1_SIZE);
    for (int i = 0; i < nnue::NNUE::HIDDEN1_SIZE; ++i) {
        hidden1White[i] = network->getInputBiases()[i];
    }
    for (int featureIdx : featuresWhite) {
        if (featureIdx >= 0 && featureIdx < nnue::NNUE::INPUT_SIZE) {
            const auto& weights = network->getInputWeights()[featureIdx];
            for (int j = 0; j < nnue::NNUE::HIDDEN1_SIZE; ++j) {
                hidden1White[j] += weights[j];
            }
        }
    }
    // Apply ReLU
    for (int i = 0; i < nnue::NNUE::HIDDEN1_SIZE; ++i) {
        hidden1White[i] = (hidden1White[i] > 0.0f) ? hidden1White[i] : 0.0f;
    }
    
    // Compute hidden1 for black perspective
    hidden1Black.resize(nnue::NNUE::HIDDEN1_SIZE);
    for (int i = 0; i < nnue::NNUE::HIDDEN1_SIZE; ++i) {
        hidden1Black[i] = network->getInputBiases()[i];
    }
    for (int featureIdx : featuresBlack) {
        if (featureIdx >= 0 && featureIdx < nnue::NNUE::INPUT_SIZE) {
            const auto& weights = network->getInputWeights()[featureIdx];
            for (int j = 0; j < nnue::NNUE::HIDDEN1_SIZE; ++j) {
                hidden1Black[j] += weights[j];
            }
        }
    }
    // Apply ReLU
    for (int i = 0; i < nnue::NNUE::HIDDEN1_SIZE; ++i) {
        hidden1Black[i] = (hidden1Black[i] > 0.0f) ? hidden1Black[i] : 0.0f;
    }
    
    // Combine perspectives: white - black
    std::vector<float> combinedHidden1(nnue::NNUE::HIDDEN1_SIZE);
    for (int i = 0; i < nnue::NNUE::HIDDEN1_SIZE; ++i) {
        combinedHidden1[i] = hidden1White[i] - hidden1Black[i];
    }
    
    // Compute hidden2
    hidden2.resize(nnue::NNUE::HIDDEN2_SIZE);
    for (int i = 0; i < nnue::NNUE::HIDDEN2_SIZE; ++i) {
        hidden2[i] = network->getHidden1Biases()[i];
    }
    for (int i = 0; i < nnue::NNUE::HIDDEN1_SIZE; ++i) {
        for (int j = 0; j < nnue::NNUE::HIDDEN2_SIZE; ++j) {
            hidden2[j] += combinedHidden1[i] * network->getHidden1Weights()[i][j];
        }
    }
    // Apply ReLU
    for (int i = 0; i < nnue::NNUE::HIDDEN2_SIZE; ++i) {
        hidden2[i] = (hidden2[i] > 0.0f) ? hidden2[i] : 0.0f;
    }
    
    // Compute output
    float output = network->getOutputBias();
    for (int i = 0; i < nnue::NNUE::HIDDEN2_SIZE; ++i) {
        output += hidden2[i] * network->getOutputWeights()[i];
    }
    
    return output;
}

void NNUETrainer::backwardPass(const TrainingPosition& position,
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
                               float& outputBiasGrad) {
    // Initialize gradients to zero
    inputWeightsGrad.resize(nnue::NNUE::INPUT_SIZE);
    for (int i = 0; i < nnue::NNUE::INPUT_SIZE; ++i) {
        inputWeightsGrad[i].resize(nnue::NNUE::HIDDEN1_SIZE, 0.0f);
    }
    inputBiasesGrad.resize(nnue::NNUE::HIDDEN1_SIZE, 0.0f);
    
    hidden1WeightsGrad.resize(nnue::NNUE::HIDDEN1_SIZE);
    for (int i = 0; i < nnue::NNUE::HIDDEN1_SIZE; ++i) {
        hidden1WeightsGrad[i].resize(nnue::NNUE::HIDDEN2_SIZE, 0.0f);
    }
    hidden1BiasesGrad.resize(nnue::NNUE::HIDDEN2_SIZE, 0.0f);
    
    outputWeightsGrad.resize(nnue::NNUE::HIDDEN2_SIZE, 0.0f);
    outputBiasGrad = 0.0f;
    
    // Output layer gradient: dL/doutput = 2 * (prediction - target) for MSE
    float outputGrad = 2.0f * (prediction - target);
    
    // Output bias gradient
    outputBiasGrad = outputGrad;
    
    // Output weights gradient
    for (int i = 0; i < nnue::NNUE::HIDDEN2_SIZE; ++i) {
        outputWeightsGrad[i] = outputGrad * hidden2[i];
    }
    
    // Hidden2 layer gradients (backprop through ReLU)
    std::vector<float> hidden2Grad(nnue::NNUE::HIDDEN2_SIZE, 0.0f);
    for (int i = 0; i < nnue::NNUE::HIDDEN2_SIZE; ++i) {
        if (hidden2[i] > 0.0f) {  // ReLU derivative: 1 if > 0, else 0
            hidden2Grad[i] = outputGrad * network->getOutputWeights()[i];
        }
    }
    
    // Hidden1 biases gradient
    for (int i = 0; i < nnue::NNUE::HIDDEN2_SIZE; ++i) {
        hidden1BiasesGrad[i] = hidden2Grad[i];
    }
    
    // Hidden1 weights gradient
    // combinedHidden1 = whiteHidden1 - blackHidden1
    std::vector<float> combinedHidden1(nnue::NNUE::HIDDEN1_SIZE);
    for (int i = 0; i < nnue::NNUE::HIDDEN1_SIZE; ++i) {
        combinedHidden1[i] = hidden1White[i] - hidden1Black[i];
    }
    
    for (int i = 0; i < nnue::NNUE::HIDDEN1_SIZE; ++i) {
        for (int j = 0; j < nnue::NNUE::HIDDEN2_SIZE; ++j) {
            hidden1WeightsGrad[i][j] = hidden2Grad[j] * combinedHidden1[i];
        }
    }
    
    // Combined hidden1 gradient (before perspective split)
    std::vector<float> combinedHidden1Grad(nnue::NNUE::HIDDEN1_SIZE, 0.0f);
    for (int i = 0; i < nnue::NNUE::HIDDEN1_SIZE; ++i) {
        for (int j = 0; j < nnue::NNUE::HIDDEN2_SIZE; ++j) {
            combinedHidden1Grad[i] += hidden2Grad[j] * network->getHidden1Weights()[i][j];
        }
    }
    
    // Split gradient to white and black perspectives
    // combinedHidden1 = whiteHidden1 - blackHidden1
    // dL/dwhiteHidden1 = dL/dcombinedHidden1 * 1
    // dL/dblackHidden1 = dL/dcombinedHidden1 * (-1)
    std::vector<float> whiteHidden1Grad(nnue::NNUE::HIDDEN1_SIZE);
    std::vector<float> blackHidden1Grad(nnue::NNUE::HIDDEN1_SIZE);
    for (int i = 0; i < nnue::NNUE::HIDDEN1_SIZE; ++i) {
        whiteHidden1Grad[i] = combinedHidden1Grad[i];
        blackHidden1Grad[i] = -combinedHidden1Grad[i];
    }
    
    // Backprop through ReLU for hidden1
    for (int i = 0; i < nnue::NNUE::HIDDEN1_SIZE; ++i) {
        if (hidden1White[i] <= 0.0f) whiteHidden1Grad[i] = 0.0f;
        if (hidden1Black[i] <= 0.0f) blackHidden1Grad[i] = 0.0f;
    }
    
    // Input biases gradient (accumulated from both perspectives)
    for (int i = 0; i < nnue::NNUE::HIDDEN1_SIZE; ++i) {
        inputBiasesGrad[i] = whiteHidden1Grad[i] + blackHidden1Grad[i];
    }
    
    // Input weights gradient (only for active features)
    Game game;
    game.setPosition(position.fen);
    auto featuresWhite = nnue::FeatureExtractor::extractFeatures(game.board, 0);
    auto featuresBlack = nnue::FeatureExtractor::extractFeatures(game.board, 1);
    
    for (int featureIdx : featuresWhite) {
        if (featureIdx >= 0 && featureIdx < nnue::NNUE::INPUT_SIZE) {
            for (int j = 0; j < nnue::NNUE::HIDDEN1_SIZE; ++j) {
                inputWeightsGrad[featureIdx][j] += whiteHidden1Grad[j];
            }
        }
    }
    
    for (int featureIdx : featuresBlack) {
        if (featureIdx >= 0 && featureIdx < nnue::NNUE::INPUT_SIZE) {
            for (int j = 0; j < nnue::NNUE::HIDDEN1_SIZE; ++j) {
                inputWeightsGrad[featureIdx][j] += blackHidden1Grad[j];
            }
        }
    }
}

void NNUETrainer::updateWeights(const std::vector<std::vector<float>>& inputWeightsGrad,
                                const std::vector<float>& inputBiasesGrad,
                                const std::vector<std::vector<float>>& hidden1WeightsGrad,
                                const std::vector<float>& hidden1BiasesGrad,
                                const std::vector<float>& outputWeightsGrad,
                                float outputBiasGrad,
                                int batchSize) {
    // Average gradients over batch
    float scale = 1.0f / batchSize;
    
    if (config.useAdam) {
        // Adam optimizer
        optimizerStep++;
        
        // Bias correction terms
        float beta1_t = std::pow(config.adamBeta1, optimizerStep);
        float beta2_t = std::pow(config.adamBeta2, optimizerStep);
        
        // Update input weights
        for (int i = 0; i < nnue::NNUE::INPUT_SIZE; ++i) {
            for (int j = 0; j < nnue::NNUE::HIDDEN1_SIZE; ++j) {
                float grad = inputWeightsGrad[i][j] * scale;
                inputWeightsM[i][j] = config.adamBeta1 * inputWeightsM[i][j] + (1.0f - config.adamBeta1) * grad;
                inputWeightsV[i][j] = config.adamBeta2 * inputWeightsV[i][j] + (1.0f - config.adamBeta2) * grad * grad;
                
                float mHat = inputWeightsM[i][j] / (1.0f - beta1_t);
                float vHat = inputWeightsV[i][j] / (1.0f - beta2_t);
                
                network->getInputWeights()[i][j] -= config.learningRate * mHat / (std::sqrt(vHat) + config.adamEpsilon);
            }
        }
        
        // Update input biases
        for (int i = 0; i < nnue::NNUE::HIDDEN1_SIZE; ++i) {
            float grad = inputBiasesGrad[i] * scale;
            inputBiasesM[i] = config.adamBeta1 * inputBiasesM[i] + (1.0f - config.adamBeta1) * grad;
            inputBiasesV[i] = config.adamBeta2 * inputBiasesV[i] + (1.0f - config.adamBeta2) * grad * grad;
            
            float mHat = inputBiasesM[i] / (1.0f - beta1_t);
            float vHat = inputBiasesV[i] / (1.0f - beta2_t);
            
            network->getInputBiases()[i] -= config.learningRate * mHat / (std::sqrt(vHat) + config.adamEpsilon);
        }
        
        // Update hidden1 weights
        for (int i = 0; i < nnue::NNUE::HIDDEN1_SIZE; ++i) {
            for (int j = 0; j < nnue::NNUE::HIDDEN2_SIZE; ++j) {
                float grad = hidden1WeightsGrad[i][j] * scale;
                hidden1WeightsM[i][j] = config.adamBeta1 * hidden1WeightsM[i][j] + (1.0f - config.adamBeta1) * grad;
                hidden1WeightsV[i][j] = config.adamBeta2 * hidden1WeightsV[i][j] + (1.0f - config.adamBeta2) * grad * grad;
                
                float mHat = hidden1WeightsM[i][j] / (1.0f - beta1_t);
                float vHat = hidden1WeightsV[i][j] / (1.0f - beta2_t);
                
                network->getHidden1Weights()[i][j] -= config.learningRate * mHat / (std::sqrt(vHat) + config.adamEpsilon);
            }
        }
        
        // Update hidden1 biases
        for (int i = 0; i < nnue::NNUE::HIDDEN2_SIZE; ++i) {
            float grad = hidden1BiasesGrad[i] * scale;
            hidden1BiasesM[i] = config.adamBeta1 * hidden1BiasesM[i] + (1.0f - config.adamBeta1) * grad;
            hidden1BiasesV[i] = config.adamBeta2 * hidden1BiasesV[i] + (1.0f - config.adamBeta2) * grad * grad;
            
            float mHat = hidden1BiasesM[i] / (1.0f - beta1_t);
            float vHat = hidden1BiasesV[i] / (1.0f - beta2_t);
            
            network->getHidden1Biases()[i] -= config.learningRate * mHat / (std::sqrt(vHat) + config.adamEpsilon);
        }
        
        // Update output weights
        for (int i = 0; i < nnue::NNUE::HIDDEN2_SIZE; ++i) {
            float grad = outputWeightsGrad[i] * scale;
            outputWeightsM[i] = config.adamBeta1 * outputWeightsM[i] + (1.0f - config.adamBeta1) * grad;
            outputWeightsV[i] = config.adamBeta2 * outputWeightsV[i] + (1.0f - config.adamBeta2) * grad * grad;
            
            float mHat = outputWeightsM[i] / (1.0f - beta1_t);
            float vHat = outputWeightsV[i] / (1.0f - beta2_t);
            
            network->getOutputWeights()[i] -= config.learningRate * mHat / (std::sqrt(vHat) + config.adamEpsilon);
        }
        
        // Update output bias
        {
            float grad = outputBiasGrad * scale;
            outputBiasM = config.adamBeta1 * outputBiasM + (1.0f - config.adamBeta1) * grad;
            outputBiasV = config.adamBeta2 * outputBiasV + (1.0f - config.adamBeta2) * grad * grad;
            
            float mHat = outputBiasM / (1.0f - beta1_t);
            float vHat = outputBiasV / (1.0f - beta2_t);
            
            network->getOutputBias() -= config.learningRate * mHat / (std::sqrt(vHat) + config.adamEpsilon);
        }
    } else {
        // SGD optimizer
        // Update input weights
        for (int i = 0; i < nnue::NNUE::INPUT_SIZE; ++i) {
            for (int j = 0; j < nnue::NNUE::HIDDEN1_SIZE; ++j) {
                network->getInputWeights()[i][j] -= config.learningRate * inputWeightsGrad[i][j] * scale;
            }
        }
        
        // Update input biases
        for (int i = 0; i < nnue::NNUE::HIDDEN1_SIZE; ++i) {
            network->getInputBiases()[i] -= config.learningRate * inputBiasesGrad[i] * scale;
        }
        
        // Update hidden1 weights
        for (int i = 0; i < nnue::NNUE::HIDDEN1_SIZE; ++i) {
            for (int j = 0; j < nnue::NNUE::HIDDEN2_SIZE; ++j) {
                network->getHidden1Weights()[i][j] -= config.learningRate * hidden1WeightsGrad[i][j] * scale;
            }
        }
        
        // Update hidden1 biases
        for (int i = 0; i < nnue::NNUE::HIDDEN2_SIZE; ++i) {
            network->getHidden1Biases()[i] -= config.learningRate * hidden1BiasesGrad[i] * scale;
        }
        
        // Update output weights
        for (int i = 0; i < nnue::NNUE::HIDDEN2_SIZE; ++i) {
            network->getOutputWeights()[i] -= config.learningRate * outputWeightsGrad[i] * scale;
        }
        
        // Update output bias
        network->getOutputBias() -= config.learningRate * outputBiasGrad * scale;
    }
}

float NNUETrainer::computeLoss(float prediction, float target) {
    float diff = prediction - target;
    return diff * diff;  // MSE
}

void NNUETrainer::splitDataset(const TrainingDataset& dataset,
                               std::vector<TrainingPosition>& train,
                               std::vector<TrainingPosition>& val) {
    const auto& allPositions = dataset.getAllPositions();
    
    // Shuffle positions
    std::vector<TrainingPosition> shuffled = allPositions;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(shuffled.begin(), shuffled.end(), gen);
    
    // Split
    size_t valSize = static_cast<size_t>(shuffled.size() * config.validationSplit);
    val.assign(shuffled.begin(), shuffled.begin() + valSize);
    train.assign(shuffled.begin() + valSize, shuffled.end());
}

void NNUETrainer::trainBatch(const std::vector<TrainingPosition>& batch) {
    if (batch.empty()) return;
    
    // Initialize gradient accumulators
    std::vector<std::vector<float>> inputWeightsGrad(nnue::NNUE::INPUT_SIZE);
    for (int i = 0; i < nnue::NNUE::INPUT_SIZE; ++i) {
        inputWeightsGrad[i].resize(nnue::NNUE::HIDDEN1_SIZE, 0.0f);
    }
    std::vector<float> inputBiasesGrad(nnue::NNUE::HIDDEN1_SIZE, 0.0f);
    std::vector<std::vector<float>> hidden1WeightsGrad(nnue::NNUE::HIDDEN1_SIZE);
    for (int i = 0; i < nnue::NNUE::HIDDEN1_SIZE; ++i) {
        hidden1WeightsGrad[i].resize(nnue::NNUE::HIDDEN2_SIZE, 0.0f);
    }
    std::vector<float> hidden1BiasesGrad(nnue::NNUE::HIDDEN2_SIZE, 0.0f);
    std::vector<float> outputWeightsGrad(nnue::NNUE::HIDDEN2_SIZE, 0.0f);
    float outputBiasGrad = 0.0f;
    
    float totalLoss = 0.0f;
    
    // Process each position in batch
    for (const auto& position : batch) {
        // Forward pass
        std::vector<float> hidden1White, hidden1Black, hidden2;
        float prediction = forwardPass(position, hidden1White, hidden1Black, hidden2);
        
        // Compute loss
        float target = position.targetEval;  // Already in centipawns
        totalLoss += computeLoss(prediction, target);
        
        // Backward pass (accumulate gradients)
        std::vector<std::vector<float>> posInputWeightsGrad;
        std::vector<float> posInputBiasesGrad;
        std::vector<std::vector<float>> posHidden1WeightsGrad;
        std::vector<float> posHidden1BiasesGrad;
        std::vector<float> posOutputWeightsGrad;
        float posOutputBiasGrad;
        
        backwardPass(position, prediction, target, hidden1White, hidden1Black, hidden2,
                   posInputWeightsGrad, posInputBiasesGrad, posHidden1WeightsGrad,
                   posHidden1BiasesGrad, posOutputWeightsGrad, posOutputBiasGrad);
        
        // Accumulate gradients
        for (int i = 0; i < nnue::NNUE::INPUT_SIZE; ++i) {
            for (int j = 0; j < nnue::NNUE::HIDDEN1_SIZE; ++j) {
                inputWeightsGrad[i][j] += posInputWeightsGrad[i][j];
            }
        }
        for (int i = 0; i < nnue::NNUE::HIDDEN1_SIZE; ++i) {
            inputBiasesGrad[i] += posInputBiasesGrad[i];
        }
        for (int i = 0; i < nnue::NNUE::HIDDEN1_SIZE; ++i) {
            for (int j = 0; j < nnue::NNUE::HIDDEN2_SIZE; ++j) {
                hidden1WeightsGrad[i][j] += posHidden1WeightsGrad[i][j];
            }
        }
        for (int i = 0; i < nnue::NNUE::HIDDEN2_SIZE; ++i) {
            hidden1BiasesGrad[i] += posHidden1BiasesGrad[i];
        }
        for (int i = 0; i < nnue::NNUE::HIDDEN2_SIZE; ++i) {
            outputWeightsGrad[i] += posOutputWeightsGrad[i];
        }
        outputBiasGrad += posOutputBiasGrad;
    }
    
    // Update weights
    updateWeights(inputWeightsGrad, inputBiasesGrad, hidden1WeightsGrad,
                 hidden1BiasesGrad, outputWeightsGrad, outputBiasGrad,
                 static_cast<int>(batch.size()));
    
    // Update stats
    stats.trainLoss = totalLoss / batch.size();
}

void NNUETrainer::train(TrainingDataset& dataset) {
    // Initialize move tables
    MoveTables::instance().init();
    
    // Split dataset
    std::vector<TrainingPosition> trainData, valData;
    splitDataset(dataset, trainData, valData);
    
    std::cout << "Training dataset: " << trainData.size() << " positions\n";
    std::cout << "Validation dataset: " << valData.size() << " positions\n";
    std::cout << "Batch size: " << config.batchSize << "\n";
    std::cout << "Epochs: " << config.numEpochs << "\n";
    std::cout << "Optimizer: " << (config.useAdam ? "Adam" : "SGD") << "\n";
    std::cout << "Learning rate: " << config.learningRate << "\n\n";
    
    // Training loop
    for (int epoch = 0; epoch < config.numEpochs; ++epoch) {
        stats.epoch = epoch + 1;
        
        // Shuffle training data
        std::random_device rd;
        std::mt19937 gen(rd());
        std::shuffle(trainData.begin(), trainData.end(), gen);
        
        // Process batches
        float epochTrainLoss = 0.0f;
        int numBatches = 0;
        
        for (size_t i = 0; i < trainData.size(); i += config.batchSize) {
            size_t batchEnd = std::min(i + config.batchSize, trainData.size());
            std::vector<TrainingPosition> batch(trainData.begin() + i, trainData.begin() + batchEnd);
            
            trainBatch(batch);
            
            epochTrainLoss += stats.trainLoss;
            numBatches++;
            stats.batch = numBatches;
            stats.totalBatches = (trainData.size() + config.batchSize - 1) / config.batchSize;
            
            if (numBatches % 10 == 0) {
                std::cout << "Epoch " << (epoch + 1) << "/" << config.numEpochs
                         << ", Batch " << numBatches << "/" << stats.totalBatches
                         << ", Loss: " << std::fixed << std::setprecision(2) << stats.trainLoss << "\n";
            }
        }
        
        stats.trainLoss = epochTrainLoss / numBatches;
        
        // Validation
        float valLoss = 0.0f;
        for (const auto& position : valData) {
            std::vector<float> hidden1White, hidden1Black, hidden2;
            float prediction = forwardPass(position, hidden1White, hidden1Black, hidden2);
            valLoss += computeLoss(prediction, position.targetEval);
        }
        stats.valLoss = valLoss / valData.size();
        
        std::cout << "\nEpoch " << (epoch + 1) << "/" << config.numEpochs << " complete:\n";
        std::cout << "  Train Loss: " << std::fixed << std::setprecision(2) << stats.trainLoss << "\n";
        std::cout << "  Val Loss: " << std::fixed << std::setprecision(2) << stats.valLoss << "\n\n";
        
        // Save model if configured
        if (config.saveInterval > 0 && (epoch + 1) % config.saveInterval == 0 && !config.savePath.empty()) {
            if (network->saveModel(config.savePath)) {
                std::cout << "Model saved to " << config.savePath << "\n\n";
            }
        }
    }
    
    // Final save
    if (!config.savePath.empty()) {
        if (network->saveModel(config.savePath)) {
            std::cout << "Final model saved to " << config.savePath << "\n";
        }
    }
}

} // namespace rl
