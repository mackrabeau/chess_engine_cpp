#include "engine_interface.h"
#include <iostream>
#include <sstream>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <fcntl.h>
#include <cerrno>

EngineInterface::EngineInterface(const std::string& enginePath, const std::string& engineName)
    : enginePath(enginePath), engineName(engineName), ready(false), processHandle(nullptr)
{
}

EngineInterface::~EngineInterface() {
    quit();
}

bool EngineInterface::initialize() {
    // Create pipes for communication
    int stdin_pipe[2], stdout_pipe[2];
    
    if (pipe(stdin_pipe) == -1 || pipe(stdout_pipe) == -1) {
        std::cerr << "Failed to create pipes" << std::endl;
        return false;
    }

    pid_t pid = fork();
    if (pid == -1) {
        std::cerr << "Failed to fork process" << std::endl;
        return false;
    }

    if (pid == 0) {
        // Child process - run the engine
        close(stdin_pipe[1]);  // Close write end of stdin pipe
        close(stdout_pipe[0]); // Close read end of stdout pipe
        
        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stdout_pipe[1], STDERR_FILENO);
        
        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        
        execl(enginePath.c_str(), enginePath.c_str(), (char*)nullptr);
        std::cerr << "Failed to exec engine: " << strerror(errno) << std::endl;
        _exit(1);
    }

    // Parent process
    close(stdin_pipe[0]);  // Close read end of stdin pipe
    close(stdout_pipe[1]); // Close write end of stdout pipe
    
    // Store file descriptors
    processHandle = malloc(sizeof(int) * 2);
    ((int*)processHandle)[0] = stdin_pipe[1];  // stdin write
    ((int*)processHandle)[1] = stdout_pipe[0]; // stdout read
    
    // Keep stdout blocking for now - we'll use select/poll for timeout
    // Non-blocking can cause issues with partial reads

    // Send uci command
    if (!sendCommand("uci")) {
        std::cerr << "Failed to send uci command" << std::endl;
        return false;
    }

    // Small delay to ensure command is sent
    usleep(50000); // 50ms

    // Wait for uciok - read all lines until we find it
    std::string line;
    bool gotUciok = false;
    auto start = std::chrono::steady_clock::now();
    int linesRead = 0;
    while (std::chrono::steady_clock::now() - start < std::chrono::seconds(5)) {
        if (readLine(line, 1000)) {
            linesRead++;
            // Debug output
            std::cerr << "[" << engineName << "] Engine response: " << line << std::endl;
            if (line.find("uciok") != std::string::npos) {
                gotUciok = true;
                break;
            }
        } else {
            // No data available, wait a bit
            usleep(10000); // 10ms
        }
    }

    if (!gotUciok) {
        std::cerr << "Engine did not respond with uciok after reading " << linesRead << " lines" << std::endl;
        if (!line.empty()) {
            std::cerr << "Last line received: " << line << std::endl;
        }
        return false;
    }

    // Send isready
    if (!sendCommand("isready")) {
        return false;
    }

    // Wait for readyok
    start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::seconds(5)) {
        if (readLine(line, 500)) {
            if (line.find("readyok") != std::string::npos) {
                ready = true;
                return true;
            }
        } else {
            usleep(10000);
        }
    }

    std::cerr << "Engine did not respond with readyok" << std::endl;
    return false;
}

bool EngineInterface::newGame() {
    if (!ready) return false;
    return sendCommand("ucinewgame");
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

    if (!sendCommand(command)) {
        return false;
    }
    
    // Flush any responses to ensure position is processed
    // Some engines may output info after position command
    std::string line;
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(100)) {
        if (readLine(line, 50)) {
            // Discard any info messages
            if (line.find("info ") == 0 || line.find("Error") == 0) {
                // Continue reading
            } else {
                // Unexpected response, but continue
            }
        } else {
            break;
        }
    }
    
    return true;
}

bool EngineInterface::getBestMove(int movetimeMs, std::string& bestMove, SearchStats& stats) {
    if (!ready) {
        bestMove = "";
        return false;
    }

    // Reset stats
    stats = SearchStats();
    bestMove = "";

    // Send go command
    std::ostringstream cmd;
    cmd << "go movetime " << movetimeMs;
    if (!sendCommand(cmd.str())) {
        return false;
    }

    // Read responses until we get bestmove
    // Use a more aggressive reading strategy - read all available data
    std::string line;
    auto start = std::chrono::steady_clock::now();
    auto timeout = std::chrono::milliseconds(movetimeMs + 5000); // Add 5 second buffer for mate searches
    
    // Track if we've seen a mate score - if so, we might need more time
    bool seenMateScore = false;
    int consecutiveEmptyReads = 0;
    const int MAX_EMPTY_READS = 100; // Allow more empty reads for mate positions

    while (std::chrono::steady_clock::now() - start < timeout) {
        // Try to read a line - use shorter timeout but read more frequently
        if (readLine(line, 200)) {
            consecutiveEmptyReads = 0; // Reset counter on successful read
            
            // Parse info strings for stats
            if (line.find("info ") == 0) {
                parseInfoString(line, stats);
                // Check if we see a mate score
                if (stats.scoreIsMate) {
                    seenMateScore = true;
                    // When we see mate, be extra patient
                    timeout = std::chrono::milliseconds(movetimeMs + 10000); // Extend timeout
                }
            }
            // Check for bestmove
            else if (line.find("bestmove ") == 0) {
                std::istringstream iss(line);
                std::string token;
                iss >> token; // "bestmove"
                if (iss >> token) {
                    bestMove = token;
                }
                break;
            }
            // Some engines return "bestmove (none)" or similar when no moves available
            else if (line.find("bestmove (none)") != std::string::npos || 
                     line.find("bestmove none") != std::string::npos) {
                bestMove = "0000"; // Treat as resignation
                break;
            }
            
            // If we got a line, try to read more immediately (don't sleep)
            // This helps when engine is outputting many lines quickly
            continue;
        } else {
            consecutiveEmptyReads++;
            
            // If we've seen a mate score, be much more patient
            if (seenMateScore) {
                if (consecutiveEmptyReads < MAX_EMPTY_READS * 3) {
                    usleep(5000); // 5ms - shorter sleep when waiting for mate
                    continue;
                }
            } else {
                // If too many empty reads and no mate, break
                if (consecutiveEmptyReads > MAX_EMPTY_READS) {
                    break;
                }
            }
            usleep(5000); // 5ms - shorter sleep for more responsive reading
        }
    }

    if (bestMove.empty()) {
        // Check if this might be because the game is over
        // (no legal moves available)
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        std::cerr << "[" << engineName << "] No bestmove received after " << elapsed << "ms";
        if (seenMateScore) {
            std::cerr << " (mate score was seen, engine may be searching deeply)";
        }
        std::cerr << std::endl;
        
        // If we saw a mate score, try reading one more time with a longer timeout
        // Sometimes the bestmove comes right after the mate info
        if (seenMateScore) {
            std::cerr << "[" << engineName << "] Attempting one more read for mate position..." << std::endl;
            if (readLine(line, 2000)) {
                if (line.find("bestmove ") == 0) {
                    std::istringstream iss(line);
                    std::string token;
                    iss >> token; // "bestmove"
                    if (iss >> token) {
                        bestMove = token;
                        std::cerr << "[" << engineName << "] Found bestmove on retry: " << bestMove << std::endl;
                        return true;
                    }
                }
            }
        }
        
        return false;
    }

    return true;
}

void EngineInterface::parseInfoString(const std::string& line, SearchStats& stats) {
    std::istringstream iss(line);
    std::string token;
    
    while (iss >> token) {
        if (token == "depth") {
            iss >> stats.depth;
        } else if (token == "nodes") {
            iss >> stats.nodes;
        } else if (token == "score") {
            iss >> token; // "cp" or "mate"
            if (token == "cp") {
                iss >> stats.score;
                stats.scoreIsMate = false;
            } else if (token == "mate") {
                iss >> stats.mateIn;
                stats.scoreIsMate = true;
                // Convert mate distance to a large score
                stats.score = (stats.mateIn > 0) ? 30000 - stats.mateIn : -30000 + stats.mateIn;
            }
        }
    }
}

bool EngineInterface::sendCommand(const std::string& command) {
    if (!processHandle) return false;

    int stdinFd = ((int*)processHandle)[0];
    std::string cmd = command + "\n";
    
    ssize_t written = write(stdinFd, cmd.c_str(), cmd.length());
    if (written != (ssize_t)cmd.length()) {
        std::cerr << "Failed to write command to engine" << std::endl;
        return false;
    }
    
    fsync(stdinFd);
    return true;
}

bool EngineInterface::readLine(std::string& line, int timeoutMs) {
    if (!processHandle) return false;

    int stdoutFd = ((int*)processHandle)[1];
    line.clear();
    
    auto start = std::chrono::steady_clock::now();
    auto timeout = std::chrono::milliseconds(timeoutMs);
    
    // Use select for timeout with blocking I/O
    fd_set readfds;
    struct timeval tv;
    
    std::string buffer;
    char ch;
    
    while (std::chrono::steady_clock::now() - start < timeout) {
        FD_ZERO(&readfds);
        FD_SET(stdoutFd, &readfds);
        
        auto elapsed = std::chrono::steady_clock::now() - start;
        auto remaining = timeout - std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
        if (remaining.count() <= 0) break;
        
        tv.tv_sec = remaining.count() / 1000;
        tv.tv_usec = (remaining.count() % 1000) * 1000;
        
        int ret = select(stdoutFd + 1, &readfds, nullptr, nullptr, &tv);
        if (ret > 0 && FD_ISSET(stdoutFd, &readfds)) {
            ssize_t n = read(stdoutFd, &ch, 1);
            if (n > 0) {
                if (ch == '\n') {
                    // Complete line
                    line = buffer;
                    // Remove carriage return if present
                    if (!line.empty() && line.back() == '\r') {
                        line.pop_back();
                    }
                    return true;
                } else if (ch != '\r') {
                    // Ignore carriage returns, add other chars
                    buffer += ch;
                }
            } else if (n == 0) {
                // EOF
                break;
            } else {
                // Error
                if (errno != EINTR) {
                    break;
                }
            }
        } else if (ret == 0) {
            // Timeout
            break;
        } else if (ret < 0) {
            if (errno == EINTR) {
                continue; // Interrupted, try again
            }
            // Error
            break;
        }
    }
    
    return false;
}

void EngineInterface::flushOutput() {
    if (!processHandle) return;
    
    std::string line;
    // Read and discard any remaining output
    while (readLine(line, 100)) {
        // Discard
    }
}

void EngineInterface::quit() {
    if (processHandle) {
        sendCommand("quit");
        flushOutput();
        
        int stdinFd = ((int*)processHandle)[0];
        int stdoutFd = ((int*)processHandle)[1];
        
        close(stdinFd);
        close(stdoutFd);
        
        free(processHandle);
        processHandle = nullptr;
    }
    ready = false;
}
