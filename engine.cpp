#include "board.h"
#include "game.h"
#include "move.h"
#include "evaluation.h"
#include "transposition.h"
#include "search.h"

#include <iostream>
#include <sstream>
#include <chrono>

using namespace std;

std::chrono::steady_clock::time_point g_searchStartTime;
long g_timeLimit = 20000;  // ms


std::string getBestMove(Game& game, int maxTimeMs = 2000) {

    game.enableFastMode();
    g_searchStartTime = std::chrono::steady_clock::now();
    g_timeLimit = maxTimeMs;
    resetSearchStats();

    Move bestMove;

    for (int depth = 1; depth <= MAX_SEARCH_DEPTH; ++depth) {
        
        string depthStr = "DEPTH:" + std::to_string(depth);
        std::cerr << depthStr << std::endl;

        Move depthBestMove = searchAtDepth(game, depth);

        if (isTimeUp()) break;

        if (depthBestMove.getMove()) {
            bestMove = depthBestMove;
        }
    }

    game.disableFastMode();
    printSearchStats();

    // no move found
    if (!(bestMove.getMove())) {
        GameState state = game.calculateGameState();
        std::string gameStateStr;
        switch (state) {
            case ONGOING: gameStateStr = "ongoing"; break;
            case CHECKMATE: gameStateStr = "checkmate"; break;
            case STALEMATE: gameStateStr = "stalemate"; break;
            case DRAW_REPETITION: gameStateStr = "draw_repetition"; break;
            case DRAW_50_MOVE: gameStateStr = "draw_50_move"; break;
            case DRAW_INSUFFICIENT_MATERIAL: gameStateStr = "draw_insufficient_material"; break;
            default: gameStateStr = "unknown"; break;
        }

        return gameStateStr;
    }

    return bestMove.toString();
}


int main() {
    MoveTables::instance().init(); // Initialize once
    Game game("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"); // Initial state

    std::string line;
    while (std::getline(std::cin, line)) {

        std::istringstream iss(line);

        std::string requestId, command;
        iss >> requestId >> command;

        if (command == "quit") {
            break;

        } else if (command == "print") {
            // prints board in terminl
            game.board.displayBoard();

        } else if (command == "set_position") {
            std::string fen;
            iss >> fen;
            game.setPosition(fen);
            std::cout << requestId << " ok" << std::endl;
            
        } else if (command == "reset") {

            game.reset();
            g_transpositionTable.clear();  // Clear TT on reset
            std::cout << requestId << " ok" << std::endl;

        } else if (command == "best") {
            std::string bestMove = getBestMove(game);

            std::cout << requestId << " " << bestMove << std::endl;

        } else if (command == "move") {
            std::string moveStr;
            iss >> moveStr;

            if (moveStr.length() < 4) {
                std::cout << requestId << " error: invalid move string" << std::endl;
                continue;
            }

            int fromSquare = (moveStr[0] - 'a') + (moveStr[1] - '1') * 8;
            int toSquare = (moveStr[2] - 'a') + (moveStr[3] - '1') * 8;

            if (!(game.isLegal(fromSquare, toSquare))) {
                std::cout << requestId << " error: illegal move" << std::endl;
                continue;
            }

            enumPiece promoPiece = nEmpty;
            if (moveStr.length() == 5) {
                char promoChar = moveStr[4];
                switch (promoChar) {
                    case 'n': promoPiece = nKnights; break;
                    case 'b': promoPiece = nBishops; break;
                    case 'r': promoPiece = nRooks;   break;
                    case 'q': promoPiece = nQueens;  break;
                    default:
                        std::cout << requestId << " error: invalid promotion piece" << std::endl;
                        continue;
                }
            } 

            Move move(fromSquare, toSquare, game.board.getEnPassantSquare(), game.board.getPieceType(fromSquare), game.board.getPieceType(toSquare), promoPiece);
            game.pushMove(move);

            std::cout << requestId << " " << game.board.toString() << std::endl;

        } else if (command == "state") {
            GameState state = game.calculateGameState();
            std::string gameStateStr;
            switch (state) {
                case ONGOING: gameStateStr = "ongoing"; break;
                case CHECKMATE: gameStateStr = "checkmate"; break;
                case STALEMATE: gameStateStr = "stalemate"; break;
                case DRAW_REPETITION: gameStateStr = "draw_repetition"; break;
                case DRAW_50_MOVE: gameStateStr = "draw_50_move"; break;
                case DRAW_INSUFFICIENT_MATERIAL: gameStateStr = "draw_insufficient_material"; break;
                default: gameStateStr = "unknown"; break;
            }

            std::cout << requestId << " " << gameStateStr << std::endl;
   
        } else if (command == "eval") {
            
            int evaluation = evaluateBoard(game.board);
            std::cout << requestId << " " << evaluation << std::endl;

        } else if (command == "position") {
            // Return current board position as FEN
            std::cout << requestId << " " << game.board.toString() << std::endl;

        } else if (command == "tt_stats"){
            double usage = g_transpositionTable.getUsage();
            size_t size = g_transpositionTable.getSize();
            std::cout << requestId << " size:" << size << " usage:" << usage << "%" << std::endl;

        } else if (command == "debug_best_move") {

            MovesStruct moves = game.generateAllLegalMoves();
            std::cerr << "=== BEST MOVE DEBUG ===" << std::endl;
            std::cerr << "Available moves: " << moves.getNumMoves() << std::endl;

            // show engine state
            std::cerr << "Side to move: " << ((game.board.gameInfo & 1) ? "white" : "black") << std::endl;
            std::cerr << "g_timeLimit (ms): " << g_timeLimit << " g_nodeCount(start): " << g_nodeCount << std::endl;
            std::cerr << "TT probes/hits (start): " << g_ttProbes << " / " << g_ttHits << std::endl;

            int rootDepth = MAX_SEARCH_DEPTH; // match searchAtDepth usage
            resetSearchStats();
            g_searchStartTime = std::chrono::steady_clock::now();

            Move bestMove = searchAtDepth(game, rootDepth);
            std::cerr << "searchAtDepth(" << rootDepth << ") returned: " << bestMove.toString() << std::endl;

            // Detailed per-move scoring using same convention as searchAtDepth
            std::vector<std::pair<int, Move>> allScores;
            int nodeBefore = g_nodeCount;
            U64 lastHash = game.board.getHash();

            for (int i = 0; i < moves.getNumMoves(); ++i) {
                Move move = moves.getMove(i);

                game.pushMove(move);
                int score = -alphabeta(-30000, 30000, rootDepth - 1, game);
                game.popMove();

                allScores.push_back({score, move});
                std::cerr << "Move " << i << ": " << move.toString()
                        << " Score: " << score << std::endl;
            }

            // TT stats & nodes used by this debug pass
            int nodesUsed = g_nodeCount - nodeBefore;
            std::cerr << "nodes used by debug pass: " << nodesUsed << "  total nodes: " << g_nodeCount << std::endl;
            std::cerr << "TT probes/hits (end): " << g_ttProbes << " / " << g_ttHits << std::endl;

            // sort and display top moves
            std::sort(allScores.begin(), allScores.end(),
                    [](const auto& a, const auto& b) { return a.first > b.first; });

            std::cerr << "\n=== TOP 8 MOVES ===" << std::endl;
            for (int i = 0; i < std::min(8, (int)allScores.size()); ++i) {
                std::cerr << i + 1 << ". " << allScores[i].second.toString()
                        << " (" << allScores[i].first << ")" << std::endl;
            }

            // detect simple back-and-forth: check if engine's chosen move is the inverse of opponent's last move
            if (moves.getNumMoves() > 0) {
                // try to detect last move and inverse (replace with your API if needed)
                // Example pseudo-check: if (bestMove == inverseOf(lastMove)) warn
                std::cerr << "Note: if you see the same corner rook moves repeated, add repetition detection in search." << std::endl;
            }
            std::cout << requestId << " debug_complete" << std::endl;

        } else if (command == "search_tree") {
            if (g_recordSearchTree) {
                stopAndPrintSearchTree();
            } else {
                startSearchTree();
                std::cout << requestId << " search_tree_started" << std::endl;
            }

        } else {
            std::cout << requestId << "unknown command" << std::endl;
        }

    }
    return 0;

}