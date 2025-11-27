#include "board.h"
#include "game.h"
#include "move.h"
#include "evaluation.h"
#include "transposition.h"
#include "search.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace std;

namespace {

const std::string STARTPOS_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

struct GoSettings {
    long wtime = -1;
    long btime = -1;
    long winc = 0;
    long binc = 0;
    long movetime = -1;
    long nodes = -1;
    int movestogo = 0;
    int depth = 0;
    bool infinite = false;
    bool ponder = false;
    std::vector<std::string> searchMoveStrings;
};

struct SearchResult {
    Move bestMove;
    int depthReached = 0;
};

std::thread g_searchThread;
std::atomic<bool> g_searchRunning(false);

long toLong(const std::string& value, long fallback = -1) {
    try {
        return std::stol(value);
    } catch (...) {
        return fallback;
    }
}

int toInt(const std::string& value, int fallback = 0) {
    try {
        return std::stoi(value);
    } catch (...) {
        return fallback;
    }
}

std::string trim(const std::string& text) {
    const auto start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    const auto end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

std::vector<std::string> tokenize(const std::string& line) {
    std::istringstream iss(line);
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

void joinFinishedSearchThreadIfNeeded() {
    if (g_searchThread.joinable() && !g_searchRunning.load(std::memory_order_acquire)) {
        g_searchThread.join();
        resetStopSearchFlag();
    }
}

void stopActiveSearch() {
    if (!g_searchThread.joinable()) {
        resetStopSearchFlag();
        return;
    }

    if (g_searchRunning.load(std::memory_order_acquire)) {
        requestStopSearch();
        g_searchThread.join();
    } else {
        g_searchThread.join();
    }

    g_searchRunning.store(false, std::memory_order_release);
    resetStopSearchFlag();
}

void printUciIdentification() {
    std::cout << "id name ChessCPP Engine" << std::endl;
    std::cout << "id author Mack Rabeau" << std::endl;
    std::cout << "option name Hash type spin default 64 min 4 max 4096" << std::endl;
    std::cout << "uciok" << std::endl;
}

bool applyMoveString(Game& game, const std::string& moveStr) {
    if (moveStr.size() < 4) return false;

    MovesStruct legal = game.generateAllLegalMoves();
    for (int i = 0; i < legal.getNumMoves(); ++i) {
        Move move = legal.getMove(i);
        if (move.toString() == moveStr) {
            game.pushMove(move);
            return true;
        }
    }
    return false;
}

bool handlePositionCommand(const std::vector<std::string>& tokens, Game& game) {
    if (tokens.size() < 2) return false;

    size_t index = 1;

    if (tokens[index] == "startpos") {
        game.setPosition(STARTPOS_FEN);
        ++index;
    } else if (tokens[index] == "fen") {
        if (index + 6 >= tokens.size()) {
            std::cout << "info string invalid fen supplied" << std::endl;
            return false;
        }
        std::string fen = tokens[index + 1];
        for (int i = 2; i <= 6; ++i) {
            fen += " " + tokens[index + i];
        }
        game.setPosition(fen);
        index += 7;
    } else {
        std::cout << "info string position command missing startpos/fen" << std::endl;
        return false;
    }

    if (index < tokens.size() && tokens[index] == "moves") {
        ++index;
        for (; index < tokens.size(); ++index) {
            if (!applyMoveString(game, tokens[index])) {
                std::cout << "info string illegal move " << tokens[index] << std::endl;
                return false;
            }
        }
    }
    return true;
}

GoSettings parseGoCommand(const std::vector<std::string>& tokens) {
    GoSettings settings;
    static const std::vector<std::string> keywords = {
        "searchmoves", "ponder",   "wtime", "btime", "winc", "binc",
        "movestogo",   "movetime", "depth", "nodes", "mate", "infinite"
    };

    auto isKeyword = [&](const std::string& value) {
        return std::find(keywords.begin(), keywords.end(), value) != keywords.end();
    };

    for (size_t i = 1; i < tokens.size(); ++i) {
        const std::string& token = tokens[i];

        if (token == "wtime" && i + 1 < tokens.size()) {
            settings.wtime = toLong(tokens[++i], settings.wtime);
        } else if (token == "btime" && i + 1 < tokens.size()) {
            settings.btime = toLong(tokens[++i], settings.btime);
        } else if (token == "winc" && i + 1 < tokens.size()) {
            settings.winc = toLong(tokens[++i], settings.winc);
        } else if (token == "binc" && i + 1 < tokens.size()) {
            settings.binc = toLong(tokens[++i], settings.binc);
        } else if (token == "movetime" && i + 1 < tokens.size()) {
            settings.movetime = toLong(tokens[++i], settings.movetime);
        } else if (token == "movestogo" && i + 1 < tokens.size()) {
            settings.movestogo = toInt(tokens[++i], settings.movestogo);
        } else if (token == "depth" && i + 1 < tokens.size()) {
            settings.depth = toInt(tokens[++i], settings.depth);
        } else if (token == "nodes" && i + 1 < tokens.size()) {
            settings.nodes = toLong(tokens[++i], settings.nodes);
        } else if (token == "mate" && i + 1 < tokens.size()) {
            ++i; // mate search not supported, ignore value
        } else if (token == "ponder") {
            settings.ponder = true;
        } else if (token == "infinite") {
            settings.infinite = true;
        } else if (token == "searchmoves") {
            ++i;
            for (; i < tokens.size(); ++i) {
                if (isKeyword(tokens[i])) {
                    --i;
                    break;
                }
                settings.searchMoveStrings.push_back(tokens[i]);
            }
        }
    }
    return settings;
}

long computeTimeLimitMs(const GoSettings& settings, const Game& game) {
    if (settings.movetime > 0) {
        return settings.movetime;
    }

    if (settings.infinite || settings.ponder) {
        return std::numeric_limits<long>::max() / 4;
    }

    bool whiteToMove = (game.board.gameInfo & 1) != 0;
    long remaining = whiteToMove ? settings.wtime : settings.btime;
    long increment = whiteToMove ? settings.winc : settings.binc;

    if (remaining > 0) {
        int movesToGo = settings.movestogo > 0 ? settings.movestogo : 30;
        long slice = remaining / std::max(1, movesToGo);
        long budget = slice + increment;
        budget = std::max<long>(budget - 50, 10); // stay safe in low time
        return budget;
    }

    return 2000; // default fallback
}

std::vector<Move> resolveSearchMoves(Game& game, const std::vector<std::string>& moveStrs) {
    std::vector<Move> resolved;
    if (moveStrs.empty()) return resolved;

    MovesStruct legal = game.generateAllLegalMoves();
    for (const auto& moveStr : moveStrs) {
        bool found = false;
        for (int i = 0; i < legal.getNumMoves(); ++i) {
            Move move = legal.getMove(i);
            if (move.toString() == moveStr) {
                resolved.push_back(move);
                found = true;
                break;
            }
        }
        if (!found) {
            std::cout << "info string ignoring unknown searchmove " << moveStr << std::endl;
        }
    }
    return resolved;
}

class FastModeGuard {
public:
    explicit FastModeGuard(Game& g) : game(g) { game.enableFastMode(); }
    ~FastModeGuard() { game.disableFastMode(); }
private:
    Game& game;
};

SearchResult runIterativeSearch(Game& game, const GoSettings& settings, const std::vector<Move>& rootFilter) {
    SearchResult result;
    FastModeGuard guard(game);

    g_searchStartTime = std::chrono::steady_clock::now();
    g_timeLimit = computeTimeLimitMs(settings, game);
    setNodeLimit(settings.nodes > 0 ? settings.nodes : -1);

    resetSearchStats();
    resetStopSearchFlag();

    const int targetDepth = (settings.depth > 0) ? std::min(settings.depth, MAX_SEARCH_DEPTH) : MAX_SEARCH_DEPTH;
    const std::vector<Move>* filterPtr = rootFilter.empty() ? nullptr : &rootFilter;

    for (int depth = 1; depth <= targetDepth; ++depth) {
        Move bestAtDepth = searchAtDepth(game, depth, filterPtr);
        if (bestAtDepth.getMove()) {
            result.bestMove = bestAtDepth;
            result.depthReached = depth;
        }

        if (isTimeUp()) {
            break;
        }
    }

    printSearchStats();
    setNodeLimit(-1);
    return result;
}

void startSearch(Game& game, const GoSettings& settings, const std::vector<Move>& rootFilter) {
    joinFinishedSearchThreadIfNeeded();
    if (g_searchRunning.load(std::memory_order_acquire)) {
        std::cout << "info string search already running" << std::endl;
        return;
    }

    g_searchRunning.store(true, std::memory_order_release);
    GoSettings settingsCopy = settings;
    std::vector<Move> filterCopy = rootFilter;

    g_searchThread = std::thread([&game, settingsCopy, filterCopy]() mutable {
        SearchResult result = runIterativeSearch(game, settingsCopy, filterCopy);
        std::string bestMove = result.bestMove.getMove() ? result.bestMove.toString() : "0000";
        std::cout << "bestmove " << bestMove << std::endl;
        std::cout.flush();
        g_searchRunning.store(false, std::memory_order_release);
        resetStopSearchFlag();
    });
}

void handleSetOption(const std::string& line) {
    const auto namePos = line.find("name");
    if (namePos == std::string::npos) return;

    const auto valuePos = line.find("value", namePos);
    std::string name = trim(line.substr(namePos + 4, valuePos == std::string::npos ? std::string::npos : valuePos - (namePos + 4)));
    std::string value = valuePos == std::string::npos ? "" : trim(line.substr(valuePos + 5));

    if (name == "Hash" && !value.empty()) {
        try {
            size_t sizeMb = std::stoul(value);
            stopActiveSearch();
            g_transpositionTable.resize(sizeMb);
        } catch (const std::exception&) {
            std::cout << "info string invalid hash size " << value << std::endl;
    }
        return;
    }

    std::cout << "info string unsupported option " << name << std::endl;
}

} // namespace

int main() {
    MoveTables::instance().init();
    Game game(STARTPOS_FEN);

    std::string line;
    while (std::getline(std::cin, line)) {
        line = trim(line);
        if (line.empty()) {
            joinFinishedSearchThreadIfNeeded();
                continue;
            }

        auto tokens = tokenize(line);
        if (tokens.empty()) {
            joinFinishedSearchThreadIfNeeded();
                continue;
            }

        const std::string& command = tokens[0];

        if (command == "uci") {
            printUciIdentification();
        } else if (command == "isready") {
            stopActiveSearch();
            std::cout << "readyok" << std::endl;
        } else if (command == "ucinewgame") {
            stopActiveSearch();
            game.reset();
            g_transpositionTable.clear();
        } else if (command == "position") {
            stopActiveSearch();
            if (!handlePositionCommand(tokens, game)) {
                std::cout << "info string failed to set position" << std::endl;
            }
        } else if (command == "go") {
            auto settings = parseGoCommand(tokens);
            auto rootFilter = resolveSearchMoves(game, settings.searchMoveStrings);
            startSearch(game, settings, rootFilter);
        } else if (command == "stop") {
            stopActiveSearch();
        } else if (command == "quit") {
            stopActiveSearch();
            break;
        } else if (command == "setoption") {
            handleSetOption(line);
        } else if (command == "ponderhit") {
            std::cout << "info string ponderhit not supported" << std::endl;
        } else {
            std::cout << "info string unknown command " << command << std::endl;
        }

        joinFinishedSearchThreadIfNeeded();
    }

    stopActiveSearch();
    return 0;
}