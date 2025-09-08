#include "transposition.h"
#include <cstring>
#include <iostream>

// gloabl instance
TranspositionTable g_transpositionTable;

TranspositionTable::TranspositionTable(size_t sizeInMB) : entries(nullptr), size(0) {
    resize(sizeInMB);
}

TranspositionTable::~TranspositionTable() {
    delete[] entries;
}

void TranspositionTable::resize(size_t sizeInMB) {
    delete[] entries;

    size_t bytesAvailable = sizeInMB * 1024 * 1024;
    size_t entriesRequested = bytesAvailable / sizeof(TTEntry);

    size = 1;
    while (size * 2 <= entriesRequested) {
        size *= 2;
    }
    sizeMask = size - 1;

    entries = new TTEntry[size];
    clear();

     std::cerr << "TT initialized: " << (size * sizeof(TTEntry)) / (1024 * 1024) << "MB (" << size << " entries)" << std::endl;
}

void TranspositionTable::clear() {
    if (entries) {
        std::memset(entries, 0, size * sizeof(TTEntry));
    }
}

bool TranspositionTable::probe(U64 key, int alpha, int beta, int depth, int& score, Move& bestMove) {
    size_t index = key & sizeMask;
    TTEntry& entry = entries[index];

    if (entry.key != key) {
        return false; // not found
    }

    bestMove = entry.bestMove;

    if (entry.depth < depth) {
        return false; // not deep enough
    }

    int storedScore = entry.score;

    switch (entry.flag) {
        case TT_EXACT:
            score = storedScore;
            return true;  // Exact score - always usable
            
        case TT_LOWER:  // Lower bound (score >= beta)
            if (storedScore >= beta) {
                score = storedScore;
                return true;  // Beta cutoff
            }
            break;

        case TT_UPPER:  // Upper bound (score <= alpha)
            if (storedScore <= alpha) {
                score = storedScore;
                return true;  // Alpha cutoff
            }
            break;
    }
    
    return false;
}

void TranspositionTable::store(U64 key, int score, int depth, TTFlag flag, const Move& bestMove) {
    if (depth < 0) depth = 0; // Prevent negative depths

    size_t index = key & sizeMask;
    TTEntry& entry = entries[index];

    // replace if :
    // 1. key is empty (0)
    // 2. key matches, same position (update existing entry)
    // 3. new search is deeper
    
    if (entry.key == 0) {
        entry.key = key;
        entry.score = score;
        entry.depth = depth;
        entry.flag = flag;
        entry.bestMove = bestMove;
        return;
    }

    if (entry.key == key) {
        if (depth >= entry.depth || flag == TT_EXACT) {
            entry.key = key;
            entry.score = score;
            entry.depth = depth;
            entry.flag = flag;
            entry.bestMove = bestMove;
        }
        return;
    }

    if (depth >= entry.depth) {
        entry.key = key;
        entry.score = score;
        entry.depth = depth;
        entry.flag = flag;
        entry.bestMove = bestMove;
    }
}

double TranspositionTable::getUsage() const {
    if (!entries || size == 0) return 0.0;

    size_t used = 0;
    for (size_t i = 0; i < size; ++i) {
        if (entries[i].key != 0) {
            ++used;
        }
    }

    return (double)used / size * 100.0;
}