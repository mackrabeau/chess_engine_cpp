#ifndef SAN_CONVERTER_H
#define SAN_CONVERTER_H

#include "../game.h"
#include "../move.h"
#include <string>
#include <vector>

class SanConverter {
public:
    // Convert UCI move string to SAN notation
    // Returns empty string on error
    static std::string uciToSan(const std::string& uciMove, Game& game);
    
    // Convert a sequence of UCI moves to SAN notation
    static std::vector<std::string> uciToSanSequence(const std::vector<std::string>& uciMoves, const std::string& startFen);
};

#endif // SAN_CONVERTER_H
