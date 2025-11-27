#ifndef BITBOARD_H
#define BITBOARD_H

#include "types.h"

#include <array>
#include <cstddef>
#include <vector>

class MagicBitboard {
public:
    static MagicBitboard& instance() {
        static MagicBitboard instance;
        return instance;
    }

    void init();

    U64 rookAttacks(int square, U64 occupancy) const;
    U64 bishopAttacks(int square, U64 occupancy) const;

private:
    MagicBitboard();
    ~MagicBitboard();
    MagicBitboard(const MagicBitboard&) = delete;
    MagicBitboard& operator=(const MagicBitboard&) = delete;

    U64 rookMasks[64];
    U64 bishopMasks[64];
    U64 rookMagics[64];
    U64 bishopMagics[64];
    int rookRelevantBits[64];
    int bishopRelevantBits[64];

    std::array<std::vector<U64>, 64> rookAttackTable;
    std::array<std::vector<U64>, 64> bishopAttackTable;

    void generateMasks();
    void generateAttackTables();
    bool validateTables() const;
};


#endif // BITBOARD_H