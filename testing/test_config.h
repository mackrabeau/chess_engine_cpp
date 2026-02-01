#ifndef TEST_CONFIG_H
#define TEST_CONFIG_H

#include <string>
#include <vector>

struct TestConfig {
    std::string engine1Path;
    std::string engine1Name;
    std::string engine2Path;
    std::string engine2Name;
    int movetimeMs;
    std::string outputDir;
    int numGames;
    bool swapColors;  // Play games with both colors for each position
};

// Predefined FEN positions for testing
namespace TestPositions {
    // Standard starting position
    const std::string STARTPOS = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    
    // Common opening positions
    const std::string ITALIAN_GAME = "r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4";
    const std::string SICILIAN_DEFENSE = "rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq c6 0 2";
    const std::string FRENCH_DEFENSE = "rnbqkbnr/pppp1ppp/4p3/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2";
    const std::string RUY_LOPEZ = "r1bqkb1r/pppp1ppp/2n2n2/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4";
    
    // Tactical positions
    const std::string TACTICAL_1 = "r1bqkb1r/pppp1Qpp/2n2n2/2B1p3/4P3/8/PPPP1PPP/RNB1K1NR b KQkq - 0 4";
    const std::string TACTICAL_2 = "r1bq1rk1/pppp1ppp/2n2n2/2b1p3/2B1P3/3P1N2/PPP2PPP/RNBQ1RK1 w - - 4 5";
    
    // Endgame positions
    const std::string ENDGAME_1 = "8/8/8/8/8/4K3/4P3/7k w - - 0 1";
    const std::string ENDGAME_2 = "8/8/8/8/8/5K2/5P2/7k w - - 0 1";
    const std::string ENDGAME_3 = "8/8/8/8/8/3K4/3P4/7k w - - 0 1";
    
    // Middle game positions
    const std::string MIDDLEGAME_1 = "r1bqkb1r/pp2pppp/2n2n2/3p4/2PP4/2N2N2/PP2PPPP/R1BQKB1R w KQkq - 0 5";
    const std::string MIDDLEGAME_2 = "r1bqkb1r/pp2pppp/2n2n2/3p4/2PP4/2N2N2/PP2PPPP/R1BQKB1R b KQkq - 0 5";
    
    // Get all test positions
    inline std::vector<std::string> getAllPositions() {
        return {
            STARTPOS,
            ITALIAN_GAME,
            SICILIAN_DEFENSE,
            FRENCH_DEFENSE,
            RUY_LOPEZ,
            TACTICAL_1,
            TACTICAL_2,
            ENDGAME_1,
            ENDGAME_2,
            ENDGAME_3,
            MIDDLEGAME_1,
            MIDDLEGAME_2
        };
    }
}

#endif // TEST_CONFIG_H
