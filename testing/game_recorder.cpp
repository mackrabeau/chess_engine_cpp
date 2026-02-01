#include "game_recorder.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef _WIN32
#include <direct.h>
#endif

GameRecorder::GameRecorder(const std::string& outputDir) 
    : outputDir(outputDir), gameCount(0)
{
    // Create output directory if it doesn't exist
    struct stat info;
    if (stat(outputDir.c_str(), &info) != 0) {
        // Directory doesn't exist, create it
        #ifdef _WIN32
            _mkdir(outputDir.c_str());
        #else
            mkdir(outputDir.c_str(), 0755);
        #endif
    }
    
    // Open CSV file
    std::string csvPath = outputDir + "/games.csv";
    csvFile.open(csvPath, std::ios::out | std::ios::trunc);
    if (!csvFile.is_open()) {
        std::cerr << "Failed to open CSV file: " << csvPath << std::endl;
        return;
    }
    
    // Open PGN file
    std::string pgnPath = outputDir + "/games.pgn";
    pgnFile.open(pgnPath, std::ios::out | std::ios::trunc);
    if (!pgnFile.is_open()) {
        std::cerr << "Failed to open PGN file: " << pgnPath << std::endl;
        return;
    }
    
    writeCsvHeader();
}

GameRecorder::~GameRecorder() {
    close();
}

void GameRecorder::recordGame(const GameResult& result, int gameId) {
    writeCsvRow(result);
    writePgnGame(result, gameId);
    gameCount++;
}

void GameRecorder::close() {
    if (csvFile.is_open()) {
        csvFile.close();
    }
    if (pgnFile.is_open()) {
        pgnFile.close();
    }
}

void GameRecorder::writeCsvHeader() {
    csvFile << "result,engine_white,engine_black,fen,pgn,depth,nodes,evaluation\n";
}

void GameRecorder::writeCsvRow(const GameResult& result) {
    // result
    csvFile << escapeCsv(result.result) << ",";
    
    // engine_white
    csvFile << escapeCsv(result.engineWhite) << ",";
    
    // engine_black
    csvFile << escapeCsv(result.engineBlack) << ",";
    
    // fen
    csvFile << escapeCsv(result.startingFen) << ",";
    
    // pgn (full game notation)
    std::string pgn = formatPgnMoves(result);
    csvFile << escapeCsv(pgn) << ",";
    
    // depth (max depth reached)
    csvFile << result.maxDepth << ",";
    
    // nodes (total nodes searched)
    csvFile << result.totalNodes << ",";
    
    // evaluation (final evaluation)
    csvFile << result.finalEvaluation << "\n";
    
    csvFile.flush();
}

void GameRecorder::writePgnGame(const GameResult& result, int gameId) {
    // PGN header
    pgnFile << "[Event \"Engine Test\"]\n";
    pgnFile << "[Site \"?\"]\n";
    pgnFile << "[Date \"?\"]\n";
    pgnFile << "[Round \"?\"]\n";
    pgnFile << "[White \"" << result.engineWhite << "\"]\n";
    pgnFile << "[Black \"" << result.engineBlack << "\"]\n";
    pgnFile << "[Result \"" << result.result << "\"]\n";
    pgnFile << "[FEN \"" << result.startingFen << "\"]\n";
    pgnFile << "[GameId \"" << gameId << "\"]\n";
    pgnFile << "\n";
    
    // Moves
    std::string moves = formatPgnMoves(result);
    pgnFile << moves << " " << result.result << "\n\n";
    
    pgnFile.flush();
}

std::string GameRecorder::formatPgnMoves(const GameResult& result) {
    std::ostringstream pgn;
    int moveNum = 1;
    bool needMoveNumber = true;
    
    for (size_t i = 0; i < result.moves.size(); ++i) {
        const auto& moveData = result.moves[i];
        
        if (moveData.isWhite) {
            if (needMoveNumber) {
                pgn << moveNum << ". ";
                needMoveNumber = false;
            }
            pgn << moveData.sanMove;
        } else {
            if (!needMoveNumber) {
                pgn << " ";
            }
            pgn << moveData.sanMove;
            moveNum++;
            needMoveNumber = true;
        }
        
        if (i < result.moves.size() - 1) {
            pgn << " ";
        }
    }
    
    return pgn.str();
}

std::string GameRecorder::escapeCsv(const std::string& str) {
    // If string contains comma, quote, or newline, wrap in quotes and escape quotes
    if (str.find(',') != std::string::npos || 
        str.find('"') != std::string::npos || 
        str.find('\n') != std::string::npos) {
        std::ostringstream escaped;
        escaped << "\"";
        for (char c : str) {
            if (c == '"') {
                escaped << "\"\"";
            } else {
                escaped << c;
            }
        }
        escaped << "\"";
        return escaped.str();
    }
    return str;
}
