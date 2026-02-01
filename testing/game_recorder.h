#ifndef GAME_RECORDER_H
#define GAME_RECORDER_H

#include "game_runner.h"
#include <string>
#include <fstream>
#include <vector>

class GameRecorder {
public:
    GameRecorder(const std::string& outputDir);
    ~GameRecorder();
    
    // Record a game result
    void recordGame(const GameResult& result, int gameId);
    
    // Close files
    void close();
    
private:
    std::string outputDir;
    std::ofstream csvFile;
    std::ofstream pgnFile;
    int gameCount;
    
    // Write CSV header
    void writeCsvHeader();
    
    // Write CSV row for a game
    void writeCsvRow(const GameResult& result);
    
    // Write PGN for a game
    void writePgnGame(const GameResult& result, int gameId);
    
    // Escape string for CSV
    std::string escapeCsv(const std::string& str);
    
    // Format PGN moves
    std::string formatPgnMoves(const GameResult& result);
};

#endif // GAME_RECORDER_H
