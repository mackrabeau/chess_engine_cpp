#include "board.h"
#include "game.h"
#include "move.h"

#include <iostream>
#include <cassert>
#include <iomanip>

using namespace std;
using namespace std::chrono;

long long perft(Game& game, int depth) {
    if (depth == 0) return 1;

    MovesStruct moves = game.generateAllLegalMoves();

    if (depth == 1) {
        return moves.getNumMoves();
    }

    long long nodes = 0;

    for (int i = 0; i < moves.getNumMoves(); ++i) {
        Move move = moves.getMove(i);
        game.pushMove(move);
        nodes += perft(game, depth - 1);
        game.popMove();
    }
    return nodes;
}

void perftN(Game& game, int depth) {
    game.enableFastMode();
    // Game game(fen);
    MovesStruct legalMoves = game.generateAllLegalMoves();

    auto totalStart = high_resolution_clock::now();
    long long totalNodes = 0;
    
    cout << "\ngo perft " << depth << endl;
    cout << "=====================================" << endl;
    for (int i = 0; i < legalMoves.getNumMoves(); ++i) {
        Move move = legalMoves.getMove(i);

        game.pushMove(move);
        string moveStr = move.toString();
        long long nodes = perft(game, depth - 1);
        game.popMove();

        cout << moveStr << ": " << nodes << endl;

        totalNodes += nodes;
    }
    
    game.disableFastMode();

    auto totalEnd = high_resolution_clock::now();
    auto totalDuration = duration_cast<milliseconds>(totalEnd - totalStart);

    cout << "=====================================" << endl;
    cout << "Total nodes: " << totalNodes << endl;
    cout << "Total time: " << totalDuration.count() << "ms" << endl;
    cout << "Nodes per second: " << fixed << setprecision(0) << (totalNodes * 1000.0) / totalDuration.count() << endl;
    cout << "Average time per move: " << fixed << setprecision(2) << (double)totalDuration.count() / legalMoves.getNumMoves() << "ms" << endl;

}

void verifyStandard() {
    struct Test { string fen; int depth; long long expected; };
    vector<Test> tests = {
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 5, 4865609},
        {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", 4, 4085603},

    };
    
    for (auto& test : tests) {
        Game game(test.fen);
        long long result = perft(game, test.depth);
        double ratio = (double)result / test.expected;
        cout << "Expected: " << test.expected << ", Got: " << result 
             << " (ratio: " << fixed << setprecision(3) << ratio << ")" << endl;
    }
}


int main() {   
    verifyStandard();

    Game game("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -");

    perftN(game, 4);
    
    // perftTable("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    // perftTable("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ");
    // perftTable("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");
    return 0;
}
