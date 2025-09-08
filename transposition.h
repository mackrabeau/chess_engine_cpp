#ifndef TRANSPOSITION_H
#define TRANSPOSITION_H

#include "types.h"
#include "move.h"

typedef uint64_t U64;
typedef int16_t I16;
typedef uint8_t U8;

enum TTFlag {
    TT_EXACT = 0,
    TT_LOWER = 1,
    TT_UPPER = 2
};

struct TTEntry {
    U64 key;        // Zobrist hash key (8 bytes)
    Move bestMove;  // Best move (4 bytes)
    I16 score;      // Score (2 bytes) - signed
    U8 depth;       // Search depth (1 byte)
    U8 flag;        // TTFlag (1 byte)
};

class TranspositionTable {
private:
    static const size_t DEFAULT_SIZE_MB = 64; // 64MB entries
    TTEntry* entries;
    size_t size;
    size_t sizeMask;

    
public:
    TranspositionTable(size_t sizeInMB = DEFAULT_SIZE_MB);
    ~TranspositionTable();
    
    void clear();
    void resize(size_t sizeInMB);
    
    bool probe(U64 key, int alpha, int beta, int depth, int& score, Move& bestMove);
    void store(U64 key, int score, int depth, TTFlag flag, const Move& bestMove);
    
    // Statistics
    size_t getSize() const { return size; }
    double getUsage() const;
    
    // Disable copy constructor and assignment
    TranspositionTable(const TranspositionTable&) = delete;
    TranspositionTable& operator=(const TranspositionTable&) = delete;
};

// ✅ Global TT instance
extern TranspositionTable g_transpositionTable;






#endif // TRANSPOSITION_H