#include "bitboard.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <iterator>
#include <vector>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace {

constexpr U64 RMagics[64] = {
    0x2080020500400f0ULL, 0x28444000400010ULL, 0x20000a1004100014ULL,
    0x20010c090202006ULL, 0x8408008200810004ULL, 0x1746000808002ULL,
    0x2200098000808201ULL, 0x12c0002080200041ULL, 0x104000208e480804ULL,
    0x8084014008281008ULL, 0x4200810910500410ULL, 0x100014481c20400cULL,
    0x4014a4040020808ULL, 0x401002001010a4ULL, 0x202000500010001ULL,
    0x8112808005810081ULL, 0x40902108802020ULL, 0x42002101008101ULL,
    0x459442200810c202ULL, 0x81001103309808ULL, 0x8110000080102ULL,
    0x8812806008080404ULL, 0x104020000800101ULL, 0x40a1048000028201ULL,
    0x4100ba0000004081ULL, 0x44803a4003400109ULL, 0xa010a00000030443ULL,
    0x91021a000100409ULL, 0x4201e8040880a012ULL, 0x22a000440201802ULL,
    0x30890a72000204ULL, 0x10411402a0c482ULL, 0x40004841102088ULL,
    0x40230000100040ULL, 0x40100010000a0488ULL, 0x1410100200050844ULL,
    0x100090808508411ULL, 0x1410040024001142ULL, 0x8840018001214002ULL,
    0x410201000098001ULL, 0x8400802120088848ULL, 0x2060080000021004ULL,
    0x82101002000d0022ULL, 0x1001101001008241ULL, 0x9040411808040102ULL,
    0x600800480009042ULL, 0x1a020000040205ULL, 0x4200404040505199ULL,
    0x2020081040080080ULL, 0x40a3002000544108ULL, 0x4501100800148402ULL,
    0x81440280100224ULL, 0x88008000000804ULL, 0x8084060000002812ULL,
    0x1840201000108312ULL, 0x5080202000000141ULL, 0x1042a180880281ULL,
    0x900802900c01040ULL, 0x8205104104120ULL, 0x9004220000440aULL,
    0x8029510200708ULL, 0x8008440100404241ULL, 0x2420001111000bdULL,
    0x4000882304000041ULL,
};

constexpr U64 BMagics[64] = {
    0x100420000431024ULL,  0x280800101073404ULL,  0x42000a00840802ULL,
    0xca800c0410c2ULL,     0x81004290941c20ULL,   0x400200450020250ULL,
    0x444a019204022084ULL, 0x88610802202109aULL,  0x11210a0800086008ULL,
    0x400a08c08802801ULL,  0x1301a0500111c808ULL, 0x1280100480180404ULL,
    0x720009020028445ULL,  0x91880a9000010a01ULL, 0x31200940150802b2ULL,
    0x5119080c20000602ULL, 0x242400a002448023ULL, 0x4819006001200008ULL,
    0x222c10400020090ULL,  0x302008420409004ULL,  0x504200070009045ULL,
    0x210071240c02046ULL,  0x1182219000022611ULL, 0x400c50000005801ULL,
    0x4004010000113100ULL, 0x2008121604819400ULL, 0xc4a4010000290101ULL,
    0x404a000888004802ULL, 0x8820c004105010ULL,   0x28280100908300ULL,
    0x4c013189c0320a80ULL, 0x42008080042080ULL,   0x90803000c080840ULL,
    0x2180001028220ULL,    0x1084002a040036ULL,   0x212009200401ULL,
    0x128110040c84a84ULL,  0x81488020022802ULL,   0x8c0014100181ULL,
    0x2222013020082ULL,    0xa00100002382c03ULL,  0x1000280001005c02ULL,
    0x84801010000114cULL,  0x480410048000084ULL,  0x21204420080020aULL,
    0x2020010000424a10ULL, 0x240041021d500141ULL, 0x420844000280214ULL,
    0x29084a280042108ULL,  0x84102a8080a20a49ULL, 0x104204908010212ULL,
    0x40a20280081860c1ULL, 0x3044000200121004ULL, 0x1001008807081122ULL,
    0x50066c000210811ULL,  0xe3001240f8a106ULL,   0x940c0204030020d4ULL,
    0x619204000210826aULL, 0x2010438002b00a2ULL,  0x884042004005802ULL,
    0xa90240000006404ULL,  0x500d082244010008ULL, 0x28190d00040014e0ULL,
    0x825201600c082444ULL,
};

constexpr int RBits[64] = {
    12, 11, 11, 11, 11, 11, 11, 12,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    12, 11, 11, 11, 11, 11, 11, 12,
};

static_assert(RBits[0] == 12, "Unexpected RBits[0]");
static_assert(RBits[56] == 12, "Unexpected RBits[56]");

constexpr int BBits[64] = {
    6, 5, 5, 5, 5, 5, 5, 6,
    5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 7, 7, 7, 7, 5, 5,
    5, 5, 7, 9, 9, 7, 5, 5,
    5, 5, 7, 9, 9, 7, 5, 5,
    5, 5, 7, 7, 7, 7, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5,
    6, 5, 5, 5, 5, 5, 5, 6,
};

static_assert(BBits[0] == 6, "Unexpected BBits[0]");
static_assert(BBits[63] == 6, "Unexpected BBits[63]");

inline int bitScanForward(U64 bb) {
#if defined(_MSC_VER)
    unsigned long idx;
    _BitScanForward64(&idx, bb);
    return static_cast<int>(idx);
#else
    return __builtin_ctzll(bb);
#endif
}

U64 setOccupancy(int index, int bits, U64 mask) {
    U64 occupancy = 0ULL;
    U64 bitboard = mask;
    for (int i = 0; i < bits; ++i) {
        int square = bitScanForward(bitboard);
        bitboard &= bitboard - 1;
        if (index & (1 << i)) {
            occupancy |= (1ULL << square);
        }
    }
    return occupancy;
}

U64 generateRookMask(int square) {
    U64 mask = 0ULL;
    int rank = square / 8;
    int file = square % 8;

    for (int r = rank + 1; r <= 6; ++r) mask |= 1ULL << (r * 8 + file);
    for (int r = rank - 1; r >= 1; --r) mask |= 1ULL << (r * 8 + file);
    for (int f = file + 1; f <= 6; ++f) mask |= 1ULL << (rank * 8 + f);
    for (int f = file - 1; f >= 1; --f) mask |= 1ULL << (rank * 8 + f);

    return mask;
}

U64 generateBishopMask(int square) {
    U64 mask = 0ULL;
    int rank = square / 8;
    int file = square % 8;

    for (int r = rank + 1, f = file + 1; r <= 6 && f <= 6; ++r, ++f)
        mask |= 1ULL << (r * 8 + f);
    for (int r = rank + 1, f = file - 1; r <= 6 && f >= 1; ++r, --f)
        mask |= 1ULL << (r * 8 + f);
    for (int r = rank - 1, f = file + 1; r >= 1 && f <= 6; --r, ++f)
        mask |= 1ULL << (r * 8 + f);
    for (int r = rank - 1, f = file - 1; r >= 1 && f >= 1; --r, --f)
        mask |= 1ULL << (r * 8 + f);

    return mask;
}

U64 rookAttacksOnTheFly(int square, U64 occupancy) {
    U64 attacks = 0ULL;
    int rank = square / 8;
    int file = square % 8;

    for (int r = rank + 1; r < 8; ++r) {
        int sq = r * 8 + file;
        attacks |= 1ULL << sq;
        if (occupancy & (1ULL << sq)) break;
    }
    for (int r = rank - 1; r >= 0; --r) {
        int sq = r * 8 + file;
        attacks |= 1ULL << sq;
        if (occupancy & (1ULL << sq)) break;
    }
    for (int f = file + 1; f < 8; ++f) {
        int sq = rank * 8 + f;
        attacks |= 1ULL << sq;
        if (occupancy & (1ULL << sq)) break;
    }
    for (int f = file - 1; f >= 0; --f) {
        int sq = rank * 8 + f;
        attacks |= 1ULL << sq;
        if (occupancy & (1ULL << sq)) break;
    }

    return attacks;
}

U64 bishopAttacksOnTheFly(int square, U64 occupancy) {
    U64 attacks = 0ULL;
    int rank = square / 8;
    int file = square % 8;

    for (int r = rank + 1, f = file + 1; r < 8 && f < 8; ++r, ++f) {
        int sq = r * 8 + f;
        attacks |= 1ULL << sq;
        if (occupancy & (1ULL << sq)) break;
    }
    for (int r = rank + 1, f = file - 1; r < 8 && f >= 0; ++r, --f) {
        int sq = r * 8 + f;
        attacks |= 1ULL << sq;
        if (occupancy & (1ULL << sq)) break;
    }
    for (int r = rank - 1, f = file + 1; r >= 0 && f < 8; --r, ++f) {
        int sq = r * 8 + f;
        attacks |= 1ULL << sq;
        if (occupancy & (1ULL << sq)) break;
    }
    for (int r = rank - 1, f = file - 1; r >= 0 && f >= 0; --r, --f) {
        int sq = r * 8 + f;
        attacks |= 1ULL << sq;
        if (occupancy & (1ULL << sq)) break;
    }

    return attacks;
}

constexpr bool kUse32BitMagic = true;

inline size_t magicTransform(U64 blockers, U64 magic, int bits) {
    if constexpr (kUse32BitMagic) {
        uint32_t lower = static_cast<uint32_t>(blockers);
        uint32_t upper = static_cast<uint32_t>(blockers >> 32);
        uint32_t magicLower = static_cast<uint32_t>(magic);
        uint32_t magicUpper = static_cast<uint32_t>(magic >> 32);
        uint32_t product = (lower * magicLower) ^ (upper * magicUpper);
        return product >> (32 - bits);
    } else {
        return static_cast<size_t>((blockers * magic) >> (64 - bits));
    }
}

} // namespace

MagicBitboard::MagicBitboard() {
    std::fill(std::begin(rookMasks), std::end(rookMasks), 0ULL);
    std::fill(std::begin(bishopMasks), std::end(bishopMasks), 0ULL);
}

MagicBitboard::~MagicBitboard() = default;

void MagicBitboard::init() {
    static bool initialized = false;
    if (initialized) return;
    for (int square = 0; square < 64; ++square) {
        rookMagics[square] = RMagics[square];
        bishopMagics[square] = BMagics[square];
        rookRelevantBits[square] = RBits[square];
        bishopRelevantBits[square] = BBits[square];
    }

    generateMasks();
    generateAttackTables();
    if (!validateTables()) {
        std::cerr << "Magic bitboard validation failed; falling back to on-the-fly attacks." << std::endl;
    }
    initialized = true;
}

void MagicBitboard::generateMasks() {
    for (int square = 0; square < 64; ++square) {
        rookMasks[square] = generateRookMask(square);
        bishopMasks[square] = generateBishopMask(square);
        int rookCount = __builtin_popcountll(rookMasks[square]);
        int bishopCount = __builtin_popcountll(bishopMasks[square]);
        if (rookCount != rookRelevantBits[square]) {
            std::cerr << "RBits mismatch on square " << square << " expected "
                      << rookRelevantBits[square] << " got " << rookCount << std::endl;
        }
        if (bishopCount != bishopRelevantBits[square]) {
            std::cerr << "BBits mismatch on square " << square << " expected "
                      << bishopRelevantBits[square] << " got " << bishopCount << std::endl;
        }
    }
}

void MagicBitboard::generateAttackTables() {
    for (int square = 0; square < 64; ++square) {
        int rookBits = rookRelevantBits[square];
        size_t rookEntries = 1ULL << rookBits;
        rookAttackTable[square].assign(rookEntries, 0ULL);
        std::vector<uint8_t> filled(rookEntries, 0);
        for (size_t index = 0; index < rookEntries; ++index) {
            U64 blockers = setOccupancy(static_cast<int>(index), rookBits, rookMasks[square]);
            size_t magicIndex = magicTransform(blockers, rookMagics[square], rookBits);
            U64 attacks = rookAttacksOnTheFly(square, blockers);
            if (filled[magicIndex] && rookAttackTable[square][magicIndex] != attacks) {
                std::cerr << "Rook magic collision at square " << square << std::endl;
            }
            rookAttackTable[square][magicIndex] = attacks;
            filled[magicIndex] = 1;
        }
    }

    for (int square = 0; square < 64; ++square) {
        int bishopBits = bishopRelevantBits[square];
        size_t bishopEntries = 1ULL << bishopBits;
        bishopAttackTable[square].assign(bishopEntries, 0ULL);
        std::vector<uint8_t> filled(bishopEntries, 0);
        for (size_t index = 0; index < bishopEntries; ++index) {
            U64 blockers = setOccupancy(static_cast<int>(index), bishopBits, bishopMasks[square]);
            size_t magicIndex = magicTransform(blockers, bishopMagics[square], bishopBits);
            U64 attacks = bishopAttacksOnTheFly(square, blockers);
            if (filled[magicIndex] && bishopAttackTable[square][magicIndex] != attacks) {
                std::cerr << "Bishop magic collision at square " << square << std::endl;
            }
            bishopAttackTable[square][magicIndex] = attacks;
            filled[magicIndex] = 1;
        }
    }
}

U64 MagicBitboard::rookAttacks(int square, U64 occupancy) const {
    U64 blockers = occupancy & rookMasks[square];
    size_t index = magicTransform(blockers, rookMagics[square], rookRelevantBits[square]);
    return rookAttackTable[square][index];
}

U64 MagicBitboard::bishopAttacks(int square, U64 occupancy) const {
    U64 blockers = occupancy & bishopMasks[square];
    size_t index = magicTransform(blockers, bishopMagics[square], bishopRelevantBits[square]);
    return bishopAttackTable[square][index];
}

bool MagicBitboard::validateTables() const {
    for (int square = 0; square < 64; ++square) {
        int rookBits = rookRelevantBits[square];
        int bishopBits = bishopRelevantBits[square];

        for (int idx = 0; idx < (1 << rookBits); ++idx) {
            U64 blockers = setOccupancy(idx, rookBits, rookMasks[square]);
            U64 expected = rookAttacksOnTheFly(square, blockers);
            U64 actual = rookAttacks(square, blockers);
            if (expected != actual) {
                std::cerr << "Rook validation mismatch on square " << square << std::endl;
                return false;
            }
        }

        for (int idx = 0; idx < (1 << bishopBits); ++idx) {
            U64 blockers = setOccupancy(idx, bishopBits, bishopMasks[square]);
            U64 expected = bishopAttacksOnTheFly(square, blockers);
            U64 actual = bishopAttacks(square, blockers);
            if (expected != actual) {
                std::cerr << "Bishop validation mismatch on square " << square << std::endl;
                return false;
            }
        }
    }
    return true;
}
