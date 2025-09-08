#ifndef GAME_H
#define GAME_H

#include "board.h"
#include "types.h"
#include "movetables.h"
#include "move.h"

#include <iostream>
#include <cstdint>
#include <string>
#include <vector>
#include <bitset>

typedef uint64_t U64;
typedef uint16_t U16;
typedef uint8_t U8;

using namespace std;

struct BoardState {
    U64 hash;
    U16 gameInfo;
    Move move;
};

struct HistoryNode {
    BoardState state;
    HistoryNode* prev;
    HistoryNode* next;

    HistoryNode(const BoardState& state) : state(state), prev(nullptr), next(nullptr) {}
};

enum GameState {
    ONGOING,
    CHECKMATE,
    STALEMATE,
    DRAW_REPETITION,
    DRAW_50_MOVE,
    DRAW_INSUFFICIENT_MATERIAL
};

class Game {

public:
    Board board;
    GameState state;
    bool inMoveGeneration = false; 
    
    Game(const std::string& initialFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") 
        : 
        tables(MoveTables::instance()),
        board(initialFEN), 
        historyHead(nullptr), 
        historyTail(nullptr), 
        historySize(0) ,
        cachedState(ONGOING),         
        stateNeedsUpdate(true),      
        lastStateHash(0),            
        cachedDrawState(false), 
        drawStateValid(false),      
        lastDrawCheckHash(0)  
    { 
        state = ONGOING;
    }

    ~Game() { clearHistory(); }

    void reset(){
        board = Board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"); // Reset to initial state
        clearHistory();
    }

    void setPosition(const std::string& fen) {
        board = Board(fen);
        clearHistory();
        invalidateGameState();
    }

    void pushMove(const Move& move);
    void popMove();

    void enableFastMode();
    void disableFastMode();

    bool isThreefoldRepetition() const;
    bool isFiftyMoveRule() const;
    bool isInsufficientMaterial() const {return false;};
    GameState checkForMateOrStaleMate();

    bool hasAnyLegalMove();

    MovesStruct generateAllLegalMoves(bool isCaptureOnly = false); 
    MovesStruct generatePseudoLegalMoves();

    bool isLegal(const U8 from, const U8 to);

    bool isInCheck(); // check if the current player's king is in check
    bool isInCheck(U8 colour); // check if the specified player's king is in check

    void generateMoves(enumPiece pieceType, int square, MovesStruct& pseudoMoves, bool isCaptureOnly = false);    // generates bitmap of all legal moves for that pieces

    void addMovesToStruct(MovesStruct& pseudoMoves, int square, U64 moves); // adds moves to the pseudoMoves struct, filtering out illegal moves
    void addPawnMovesToStruct(MovesStruct& pseudoMoves, int square, U64 moves); // adds moves to the pseudoMoves struct, filtering out illegal moves
  
    U64 attackedBB(U8 enemyColour); // checks if square is attacked by enemy pieces

    void displayBitboard(U64 bitboard, int square, char symbol) const;
    int historySize; // to keep track of the number of moves in history

    GameState getGameState();
    GameState calculateGameState();
    // GameState getGameStateForSearch();
    bool isPositionTerminal();
    // bool isPositionTerminal(enumPiece colour);

    // int getTerminalValue(int depth);
    bool isDrawByRule();
    void invalidateGameState();

private:
    const MoveTables& tables; // reference to move tables
    bool useStackHistory = false;

    // stack of board states for move history
    static const int MAX_GAME_HISTORY = 64; // max search depth for the engine
    BoardState searchHistory[MAX_GAME_HISTORY]; // history of board states for search
    int searchDepth = 0; // current size of the search history

    // linked list for move history
    // chache game sates, the check is expensive 
    GameState cachedState = ONGOING;
    bool stateNeedsUpdate = true;
    U64 lastStateHash = 0;

    bool cachedDrawState = false;
    bool drawStateValid = false;
    U64 lastDrawCheckHash = 0;

    HistoryNode* historyHead; // pointer to the head of the history linked list
    HistoryNode* historyTail; // pointer to the tail of the history linked list

    void clearHistory();
    void pushBoardState(const BoardState& state);
    BoardState popBoardState();

    U64 getBishopAttacks(U64 occupancy, int square);
    U64 getRookAttacks(U64 occupancy, int square);

    void generateKingMovesForSquare(int square, MovesStruct& pseudoMoves, bool isCaptureOnly = false); 
    void generatePawnMovesForSquare(int square, MovesStruct& pseudoMoves, bool isCaptureOnly = false); 
    void generateBishopMovesForSquare(int square, MovesStruct& pseudoMoves, bool isCaptureOnly = false); 
    void generateKnightMovesForSquare(int square, MovesStruct& pseudoMoves, bool isCaptureOnly = false);
    void generateRookMovesForSquare(int square, MovesStruct& pseudoMoves, bool isCaptureOnly = false); 
    void generateQueenMovesForSquare(int square, MovesStruct& pseudoMoves, bool isCaptureOnly = false);

    bool hasLegalMoveFromSquare(enumPiece pieceType, int square);
};

#endif // GAME_H