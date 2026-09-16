#include "engine_interface.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

EngineInterface::EngineInterface(const std::string& enginePath, const std::string& engineName)
    : enginePath(enginePath), engineName(engineName), ready(false), stdinFd(-1), stdoutFd(-1), childPid(-1) {}

EngineInterface::~EngineInterface() {
    quit();
}

bool EngineInterface::initialize() {
    int inPipe[2];
    int outPipe[2];
    if (pipe(inPipe) == -1 || pipe(outPipe) == -1) {
        std::cerr << "Failed to create pipes" << std::endl;
        return false;
    }

    childPid = fork();
    if (childPid == -1) {
        std::cerr << "Failed to fork process" << std::endl;
        close(inPipe[0]);
        close(inPipe[1]);
        close(outPipe[0]);
        close(outPipe[1]);
        return false;
    }

    if (childPid == 0) {
        close(inPipe[1]);
        close(outPipe[0]);
        dup2(inPipe[0], STDIN_FILENO);
        dup2(outPipe[1], STDOUT_FILENO);
        dup2(outPipe[1], STDERR_FILENO);
        close(inPipe[0]);
        close(outPipe[1]);

        execl(enginePath.c_str(), enginePath.c_str(), nullptr);
        std::cerr << "Failed to exec engine: " << strerror(errno) << std::endl;
        _exit(1);
    }

    close(inPipe[0]);
    close(outPipe[1]);
    stdinFd = inPipe[1];
    stdoutFd = outPipe[0];

    if (!sendCommand("uci")) {
        return false;
    }

    std::string line;
    const auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::seconds(5)) {
        if (readLine(line, 250)) {
            if (line.find("uciok") != std::string::npos) {
                break;
            }
        }
    }

    if (!sendCommand("isready")) {
        return false;
    }

    const auto readyStart = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - readyStart < std::chrono::seconds(5)) {
        if (readLine(line, 250)) {
            if (line.find("readyok") != std::string::npos) {
                ready = true;
                return true;
            }
        }
    }

    return false;
}

bool EngineInterface::sendCommand(const std::string& command) {
    if (stdinFd < 0) return false;
    std::string payload = command + "\n";
    const ssize_t written = write(stdinFd, payload.c_str(), payload.size());
    return written == static_cast<ssize_t>(payload.size());
}

bool EngineInterface::readLine(std::string& line, int timeoutMs) {
    if (stdoutFd < 0) return false;

    while (true) {
        const std::size_t newline = outputBuffer.find('\n');
        if (newline != std::string::npos) {
            line = outputBuffer.substr(0, newline);
            outputBuffer.erase(0, newline + 1);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            return true;
        }

        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(stdoutFd, &readSet);

        timeval timeout;
        timeout.tv_sec = timeoutMs / 1000;
        timeout.tv_usec = (timeoutMs % 1000) * 1000;

        const int readyCount = select(stdoutFd + 1, &readSet, nullptr, nullptr, &timeout);
        if (readyCount <= 0) {
            return false;
        }

        char buffer[4096];
        const ssize_t readSize = read(stdoutFd, buffer, sizeof(buffer));
        if (readSize <= 0) {
            return false;
        }
        outputBuffer.append(buffer, static_cast<std::size_t>(readSize));
    }
}

void EngineInterface::parseInfoString(const std::string& line, SearchStats& stats) {
    std::istringstream iss(line);
    std::string token;
    while (iss >> token) {
        if (token == "depth") {
            if (iss >> stats.depth) {}
        } else if (token == "nodes") {
            if (iss >> stats.nodes) {}
        } else if (token == "score") {
            if (iss >> token && token == "cp") {
                iss >> stats.score;
                stats.scoreIsMate = false;
            } else if (iss >> token && token == "mate") {
                iss >> stats.mateIn;
                stats.score = stats.mateIn * 1000;
                stats.scoreIsMate = true;
            }
        }
    }
}

void EngineInterface::flushOutput() {
    std::string line;
    while (readLine(line, 10)) {
        // discard output
    }
}

bool EngineInterface::setPosition(const std::string& fen, const std::vector<std::string>& moves) {
    if (!ready) return false;

    std::string command = "position ";
    if (fen == "startpos") {
        command += "startpos";
    } else {
        command += "fen " + fen;
    }

    if (!moves.empty()) {
        command += " moves";
        for (const auto& move : moves) {
            command += " " + move;
        }
    }

    return sendCommand(command);
}

bool EngineInterface::newGame() {
    if (!ready) return false;
    return sendCommand("ucinewgame");
}

bool EngineInterface::getBestMove(int movetimeMs, std::string& bestMove, SearchStats& stats) {
    if (!ready) return false;

    bestMove.clear();
    stats = SearchStats();
    if (!sendCommand("go movetime " + std::to_string(movetimeMs))) {
        return false;
    }

    std::string line;
    const auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(movetimeMs + 2000)) {
        if (readLine(line, 100)) {
            if (line.find("info ") == 0) {
                parseInfoString(line, stats);
            } else if (line.find("bestmove ") == 0) {
                std::istringstream iss(line);
                std::string token;
                iss >> token;
                if (iss >> token) {
                    bestMove = token;
                    return true;
                }
            }
        }
    }

    return false;
}

void EngineInterface::quit() {
    if (stdinFd >= 0) {
        sendCommand("quit");
        close(stdinFd);
        stdinFd = -1;
    }
    if (stdoutFd >= 0) {
        close(stdoutFd);
        stdoutFd = -1;
    }
    if (childPid > 0) {
        waitpid(childPid, nullptr, 0);
        childPid = -1;
    }
    outputBuffer.clear();
    ready = false;
}
