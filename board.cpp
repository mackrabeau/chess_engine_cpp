#include "board.h"
#include "movetables.h"

#include <cstring>
#include <sstream>

using namespace std;

Board::Board(const std::string& fen){
    clearBoard();
    gameInfo = 0ULL;
    hash = 0ULL; // Initialize hash to zero


    std::istringstream iss(fen);
    std::string boardPart, turnPart, castlingPart, epPart, halfmovePart, fullmovePart;

    // Parse the FEN string
    iss >> boardPart >> turnPart >> castlingPart >> epPart >> halfmovePart >> fullmovePart;

    // 1. Piece placement
    int square = 56; // Start at a8
    for (char c : boardPart) {
        if (c == '/') {
            square -= 16; // Move to next rank
        } else if (isdigit(c)) {
            square += c - '0';
        } else {
            loadPiece(c, square);
            square++;
        }
    }

    // 2. Turn
    if (turnPart == "w") {
        gameInfo |= TURN_MASK; // White to move
    } else if (turnPart == "b") {
        gameInfo &= ~TURN_MASK; // Black to move
    }

    // 3. Castling rights
    for (char c : castlingPart) {
        switch (c) {
            case 'K': gameInfo |= WK_CASTLE; break;
            case 'Q': gameInfo |= WQ_CASTLE; break;
            case 'k': gameInfo |= BK_CASTLE; break;
            case 'q': gameInfo |= BQ_CASTLE; break;
            case '-': break;
        }
    }
    // 4. Move count (halfmove clock)
    if (!halfmovePart.empty()) {
        int moveCount = std::stoi(halfmovePart);
        gameInfo &= ~MOVE_MASK;
        gameInfo |= (moveCount << 6) & MOVE_MASK;
    }
    
    // 5. En passant square
    if (epPart != "-" && !epPart.empty()) {
        char fileChar = epPart[0];
        int file = fileChar - 'a';
        // Store file in en passant bits
        gameInfo &= ~(EP_IS_SET | EP_FILE_MASK);
        gameInfo |= EP_IS_SET | ((file << EP_FILE_SHIFT) & EP_FILE_MASK);
    } else {
        // No en passant
        gameInfo &= ~(EP_IS_SET | EP_FILE_MASK); // Clear old ep info
    }

    //     // 6. Fullmove number (optional, usually not needed for move generation)
    // if (!fullmovePart.empty()) {
    //     int fullmoveNumber = std::stoi(fullmovePart);
    //     // You can store this in a member variable if you want, but it's not needed for move legality.
    // }

    calculateHash(); // Calculate the hash for the initial position
};

Board::Board(const Board& other){
    for (int i = 0; i < 8; i++){
        pieceBB[i] = other.pieceBB[i];
    }
    gameInfo = other.gameInfo;
    hash = other.hash;
}

Board::Board(U64 otherPieceBB[8], const U16& otherGameInfo, const U64& otherHash) {
    memcpy(pieceBB, otherPieceBB, 8 * sizeof(U64));
    gameInfo = otherGameInfo;
    hash = otherHash;
}

std::string Board::toString() const {

    std::string fen;
    
    // 1. Piece placement (from rank 8 to 1)
    for (int rank = 7; rank >= 0; rank--) {
        int emptyCount = 0;
        
        for (int file = 0; file < 8; file++) {
            int square = rank * 8 + file;
            char piece = pieceToChar(square);
            
            if (piece == '.') {
                emptyCount++;
            } else {
                if (emptyCount > 0) {
                    fen += std::to_string(emptyCount);
                    emptyCount = 0;
                }
                fen += piece;
            }
        }
        
        if (emptyCount > 0) {
            fen += std::to_string(emptyCount);
        }
        
        if (rank > 0) {
            fen += '/';
        }
    }
    
    // 2. Active color
    fen += ' ';
    fen += (gameInfo & TURN_MASK) ? 'w' : 'b';
    
    // 3. Castling availability
    fen += ' ';
    bool hasCastlingRights = false;
    if (gameInfo & WK_CASTLE) { fen += 'K'; hasCastlingRights = true; }
    if (gameInfo & WQ_CASTLE) { fen += 'Q'; hasCastlingRights = true; }
    if (gameInfo & BK_CASTLE) { fen += 'k'; hasCastlingRights = true; }
    if (gameInfo & BQ_CASTLE) { fen += 'q'; hasCastlingRights = true; }
    if (!hasCastlingRights) { fen += '-'; }
    
    // 4. En passant target square
    fen += ' ';
    if (gameInfo & EP_IS_SET) {
        int epFile = (gameInfo & EP_FILE_MASK) >> EP_FILE_SHIFT;
        int epRank = (gameInfo & TURN_MASK) ? 6 : 3; // If white to move, ep square is on rank 6; if black, rank 3
        
        fen += static_cast<char>('a' + epFile);
        fen += static_cast<char>('0' + epRank);
    } else {
        fen += '-';
    }
    
    // 5. Halfmove clock (for 50-move rule)
    int halfmoveClock = (gameInfo & MOVE_MASK) >> 6; // Using 6 as the shift based on code context
    fen += ' ' + std::to_string(halfmoveClock);
    
    // 6. Fullmove number +++ IMPLEMENT LATER
    // fen += "";
    return fen;
}



// zobrist hash function
void Board::calculateHash(){
    MoveTables &moveTables = MoveTables::instance();
    hash = 0ULL; // reset hash

    // hash pieces on the board 
    U64 pieces = getAllPieces();
    while (pieces) {
        int square = __builtin_ctzll(pieces);  // get the least significant set bit
        pieces &= pieces - 1;                  // remove the least significant set bit

        enumPiece piece = getPieceType(square);
        if (piece != nEmpty) {
            enumPiece colour = getColourType(square);
            hash ^= moveTables.zobristTable[getPieceIndex(piece, colour)][square];
        }
    }

    if (!(gameInfo & TURN_MASK)) { // black to move
        hash ^= moveTables.zobristSideToMove; // XOR for black to move
    }

    // castling rights
    int castlingIdx = 0;
    if (gameInfo & WK_CASTLE) castlingIdx |= 1;
    if (gameInfo & WQ_CASTLE) castlingIdx |= 2;
    if (gameInfo & BK_CASTLE) castlingIdx |= 4;
    if (gameInfo & BQ_CASTLE) castlingIdx |= 8;
    hash ^= moveTables.zobristCastling[castlingIdx];

    // en passant file;
    if (gameInfo & EP_IS_SET) {
        int epFile = (gameInfo & EP_FILE_MASK) >> EP_FILE_SHIFT;
        hash ^= moveTables.zobristEnPassant[epFile];
    }
}

void Board::clearBoard(){
    for (int i = 0; i < 8; i++) {
        pieceBB[i] = 0;  // clear all the bitboards
    }
}

void Board::displayBoard() const {
    for (int r = 7; r >= 0; r --){
        for (int c = 0; c < 8; c ++){

            int square = r * 8 + c;
            std::cout<< pieceToChar(square) << " ";
        }
        std::cout<< "\n";
    }
    // coloursTurnToString();
    displayGameInfo();
    std::cout << "------------------------\n";
}

void Board::displayGameInfo() const {
    // Turn
    std::cout << "Turn: " << ((gameInfo & TURN_MASK) ? "White" : "Black") << std::endl;

    // Castling rights
    std::cout << "Castling rights: ";
    bool any = false;
    if (gameInfo & WK_CASTLE) { std::cout << "K"; any = true; }
    if (gameInfo & WQ_CASTLE) { std::cout << "Q"; any = true; }
    if (gameInfo & BK_CASTLE) { std::cout << "k"; any = true; }
    if (gameInfo & BQ_CASTLE) { std::cout << "q"; any = true; }
    if (!any) std::cout << "-";
    std::cout << std::endl;

    // Halfmove clock
    int halfmoveClock = getHalfMoveClock();
    std::cout << "Halfmove clock: " << halfmoveClock << std::endl;

    // En passant square
    int epFile = (gameInfo & EP_FILE_MASK) >> EP_FILE_SHIFT;
    if ((gameInfo & EP_IS_SET) == 0) {
        std::cout << "En passant: -" << std::endl;
    } else {
        char fileChar = 'a' + epFile;
        int rank = (gameInfo & TURN_MASK) ? 6 : 3; // If white to move, ep is on rank 6; if black, rank 3
        std::cout << "En passant: " << fileChar << rank << std::endl;
    }
}

void Board::coloursTurnToString() const{
    // Display the current turn (1 for white, 0 for black)
    if (gameInfo & TURN_MASK) {
        std::cout << "White's turn" << std::endl;
    } else {
        std::cout << "Black's turn" << std::endl;
    }
}

void Board::loadPiece(char piece, int square) {
    
    if (piece == toupper(piece)) {
        pieceBB[nWhite] = pieceBB[nWhite] | (1ULL << square);
    } else {
        pieceBB[nBlack] = pieceBB[nBlack] | (1ULL << square);
    }

    piece = toupper(piece);
    
    if (piece == 'P') {pieceBB[nPawns] = pieceBB[nPawns] | (1ULL << square); return;}
    if (piece == 'N') {pieceBB[nKnights] = pieceBB[nKnights] | (1ULL << square); return;}
    if (piece == 'B') {pieceBB[nBishops] = pieceBB[nBishops] | (1ULL << square); return;}
    if (piece == 'R') {pieceBB[nRooks] = pieceBB[nRooks] | (1ULL << square); return;}
    if (piece == 'Q') {pieceBB[nQueens] = pieceBB[nQueens] | (1ULL << square); return;}
    if (piece == 'K') {pieceBB[nKings] = pieceBB[nKings] | (1ULL << square); return;}
}


char Board::pieceToChar(int square) const {
    if (getWhitePawns() >> square & 1){return 'P';}
    if (getBlackPawns() >> square & 1){return 'p';}

    if (getWhiteBishops() >> square & 1){return 'B';}
    if (getBlackBishops() >> square & 1){return 'b';}

    if (getWhiteKnights() >> square & 1){return 'N';}
    if (getBlackKnights() >> square & 1){return 'n';}

    if (getWhiteRooks() >> square & 1){return 'R';}
    if (getBlackRooks() >> square & 1){return 'r';}

    if (getWhiteQueens() >> square & 1){return 'Q';}
    if (getBlackQueens() >> square & 1){return 'q';}

    if (getWhiteKing() >> square & 1){return 'K';}
    if (getBlackKing() >> square & 1){return 'k';}
    return '.';
}

int Board::colourCode(enumPiece ct) const {
    return static_cast<int>(ct);
}

int Board::pieceCode(enumPiece pt) const {
    return static_cast<int>(pt);
}



