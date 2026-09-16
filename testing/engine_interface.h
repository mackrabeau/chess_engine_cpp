#ifndef ENGINE_INTERFACE_H
#define ENGINE_INTERFACE_H

#include <chrono>
#include <string>
#include <vector>

#include <sys/types.h>

struct SearchStats {
    int depth = 0;
    long nodes = 0;
    int score = 0;
    bool scoreIsMate = false;
    int mateIn = 0;
};

class EngineInterface {
public:
    EngineInterface(const std::string& enginePath, const std::string& engineName);
    ~EngineInterface();

    bool initialize();
    bool setPosition(const std::string& fen, const std::vector<std::string>& moves = {});
    bool newGame();
    bool getBestMove(int movetimeMs, std::string& bestMove, SearchStats& stats);

    std::string getName() const { return engineName; }
    bool isReady() const { return ready; }
    void quit();

private:
    std::string enginePath;
    std::string engineName;
    bool ready;
    int stdinFd;
    int stdoutFd;
    pid_t childPid;
    std::string outputBuffer;

    bool sendCommand(const std::string& command);
    bool readLine(std::string& line, int timeoutMs = 5000);
    void parseInfoString(const std::string& line, SearchStats& stats);
    void flushOutput();
};

#endif // ENGINE_INTERFACE_H
