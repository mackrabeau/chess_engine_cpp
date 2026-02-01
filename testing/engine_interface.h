#ifndef ENGINE_INTERFACE_H
#define ENGINE_INTERFACE_H

#include <string>
#include <memory>
#include <vector>
#include <chrono>

struct SearchStats {
    int depth = 0;
    long nodes = 0;
    int score = 0;  // evaluation score (centipawns)
    bool scoreIsMate = false;
    int mateIn = 0;  // if scoreIsMate, this is the mate distance
};

class EngineInterface {
public:
    EngineInterface(const std::string& enginePath, const std::string& engineName);
    ~EngineInterface();

    // Initialize the engine (send uci, isready)
    bool initialize();

    // Set position (FEN or startpos with optional moves)
    bool setPosition(const std::string& fen, const std::vector<std::string>& moves = {});
    
    // Send ucinewgame command to reset engine state
    bool newGame();

    // Get best move with time limit (milliseconds)
    // Returns empty string on error, "0000" for resign
    // Updates stats with search information
    bool getBestMove(int movetimeMs, std::string& bestMove, SearchStats& stats);

    // Get engine name
    std::string getName() const { return engineName; }

    // Check if engine is ready
    bool isReady() const { return ready; }

    // Send quit command and cleanup
    void quit();

private:
    std::string enginePath;
    std::string engineName;
    bool ready;
    
    // Subprocess handles (platform-specific)
    void* processHandle;  // FILE* for stdin/stdout on Unix
    
    // Send command to engine
    bool sendCommand(const std::string& command);
    
    // Read line from engine (with timeout)
    bool readLine(std::string& line, int timeoutMs = 5000);
    
    // Parse info string to extract stats
    void parseInfoString(const std::string& line, SearchStats& stats);
    
    // Flush any remaining output
    void flushOutput();
};

#endif // ENGINE_INTERFACE_H
