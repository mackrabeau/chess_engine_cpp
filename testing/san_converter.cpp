#include "san_converter.h"
#include "../move.h"
#include <sstream>
#include <algorithm>

std::string SanConverter::uciToSan(const std::string& uciMove, Game& game) {
    if (uciMove.length() < 4) return "";
    
    // Parse UCI move (e.g., "e2e4" or "e7e8q")
    int fromFile = uciMove[0] - 'a';
    int fromRank = uciMove[1] - '1';
    int toFile = uciMove[2] - 'a';
    int toRank = uciMove[3] - '1';
    
    if (fromFile < 0 || fromFile > 7 || fromRank < 0 || fromRank > 7 ||
        toFile < 0 || toFile > 7 || toRank < 0 || toRank > 7) {
        return "";
    }
    
    int from = fromRank * 8 + fromFile;
    int to = toRank * 8 + toFile;
    
    // Get promotion piece if present
    enumPiece promoPiece = nEmpty;
    if (uciMove.length() >= 5) {
        char promo = uciMove[4];
        switch (promo) {
            case 'q': promoPiece = nQueens; break;
            case 'r': promoPiece = nRooks; break;
            case 'b': promoPiece = nBishops; break;
            case 'n': promoPiece = nKnights; break;
            default: break;
        }
    }
    
    // Find the move in legal moves
    MovesStruct legalMoves = game.generateAllLegalMoves();
    Move foundMove = MOVE_NONE;
    
    for (int i = 0; i < legalMoves.getNumMoves(); ++i) {
        Move move = legalMoves.getMove(i);
        if (getFrom(move) == from && getTo(move) == to) {
            // Check promotion if needed
            if (promoPiece != nEmpty) {
                if (getPromotionPiece(move) == promoPiece) {
                    foundMove = move;
                    break;
                }
            } else {
                if (!isPromotion(move) && !isPromoCapture(move)) {
                    foundMove = move;
                    break;
                }
            }
        }
    }
    
    if (foundMove == MOVE_NONE) {
        return "";
    }
    
    // Generate SAN notation
    enumPiece pieceType = game.board.getPieceType(from);
    
    std::ostringstream san;
    
    // Handle castling
    if (isKingCastle(foundMove)) {
        return "O-O";
    } else if (isQueenCastle(foundMove)) {
        return "O-O-O";
    }
    
    // Handle pawn moves
    if (pieceType == nPawns) {
        if (isCapture(foundMove)) {
            // Capture: e.g., "exd5"
            san << static_cast<char>('a' + fromFile);
            san << 'x';
            san << static_cast<char>('a' + toFile);
            san << static_cast<char>('1' + toRank);
        } else {
            // Quiet move: e.g., "e4"
            san << static_cast<char>('a' + toFile);
            san << static_cast<char>('1' + toRank);
        }
        
        // Promotion
        if (isPromotion(foundMove) || isPromoCapture(foundMove)) {
            enumPiece promo = getPromotionPiece(foundMove);
            char promoChar = 'Q';
            switch (promo) {
                case nQueens: promoChar = 'Q'; break;
                case nRooks: promoChar = 'R'; break;
                case nBishops: promoChar = 'B'; break;
                case nKnights: promoChar = 'N'; break;
                default: break;
            }
            san << '=' << promoChar;
        }
    } else {
        // Piece moves
        char pieceChar = ' ';
        switch (pieceType) {
            case nKings: pieceChar = 'K'; break;
            case nQueens: pieceChar = 'Q'; break;
            case nRooks: pieceChar = 'R'; break;
            case nBishops: pieceChar = 'B'; break;
            case nKnights: pieceChar = 'N'; break;
            default: break;
        }
        
        san << pieceChar;
        
        // Check for ambiguity (multiple pieces of same type can move to same square)
        // For simplicity, we'll add file disambiguation if needed
        int ambiguousCount = 0;
        for (int i = 0; i < legalMoves.getNumMoves(); ++i) {
            Move m = legalMoves.getMove(i);
            if (getTo(m) == to && 
                game.board.getPieceType(getFrom(m)) == pieceType &&
                getFrom(m) != from) {
                ambiguousCount++;
            }
        }
        
        if (ambiguousCount > 0) {
            // Add file disambiguation
            san << static_cast<char>('a' + fromFile);
        }
        
        // Capture
        if (isCapture(foundMove)) {
            san << 'x';
        }
        
        // Destination square
        san << static_cast<char>('a' + toFile);
        san << static_cast<char>('1' + toRank);
    }
    
    // Make the move to check for check/checkmate
    game.pushMove(foundMove);
    bool inCheck = game.isInCheck();
    bool isTerminal = game.isPositionTerminal();
    game.popMove();
    
    if (isTerminal) {
        if (inCheck) {
            san << '#';
        } else {
            // Stalemate - no special notation needed
        }
    } else if (inCheck) {
        san << '+';
    }
    
    return san.str();
}

std::vector<std::string> SanConverter::uciToSanSequence(const std::vector<std::string>& uciMoves, const std::string& startFen) {
    std::vector<std::string> sanMoves;
    Game game(startFen);
    
    for (const auto& uciMove : uciMoves) {
        std::string san = uciToSan(uciMove, game);
        if (san.empty()) {
            // Error converting move
            break;
        }
        sanMoves.push_back(san);
        
        // Apply move to game
        MovesStruct legal = game.generateAllLegalMoves();
        bool found = false;
        for (int i = 0; i < legal.getNumMoves(); ++i) {
            Move move = legal.getMove(i);
            if (moveToString(move) == uciMove) {
                game.pushMove(move);
                found = true;
                break;
            }
        }
        if (!found) {
            break;
        }
    }
    
    return sanMoves;
}
