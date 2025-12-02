#include "game.h"
#include "bitboard.h"

using namespace std;

void Game::enableFastMode() {
    useStackHistory = true;
    searchHistory[0].move = MOVE_NONE;                // empty/dummy move
    searchHistory[0].gameInfo = board.gameInfo;
    searchHistory[0].hash = board.getHash();
    searchHistory[0].pieceMoved = nEmpty;          // dummy piece for empty move
    searchDepth = 1;
}

void Game::disableFastMode() {
    useStackHistory = false;
}

bool Game::hasAnyLegalMove() {
    bool oldInMoveGeneration = inMoveGeneration;
    inMoveGeneration = true; // prevent recursive calls to hasAnyLegalMove

    U64 pieces = board.getFriendlyPieces();
    U64 friendlyPieces = pieces;
    U64 enemyAttacks = attackedBB(board.enemyColour());

    while (pieces) {
        int square = __builtin_ctzll(pieces);  // get the least significant set bit
        pieces &= pieces - 1;                  // remove the least significant set bit

        enumPiece pieceType = board.getPieceType(square);
        if (pieceType != nEmpty) {
            if (hasLegalMoveFromSquare(pieceType, friendlyPieces, enemyAttacks, square)) {
                inMoveGeneration = oldInMoveGeneration;
                return true;
            }
        }
    }

    inMoveGeneration = oldInMoveGeneration;
    return false;
}

bool Game::hasLegalMoveFromSquare(enumPiece pieceType, U64& friendlyPieces, U64& enemyAttacks, int square) {

    MovesStruct legalMoves;
    int kingSquare = __builtin_ctzll( (board.friendlyColour() == nWhite) ? board.getWhiteKing() : board.getBlackKing());
    
    generateLegalMovesForPiece(pieceType, square, legalMoves, friendlyPieces, enemyAttacks, kingSquare, true); // only called while in check

    return (legalMoves.count != 0);
}

GameState Game::getGameState() {
    if (stateNeedsUpdate || board.hash != lastStateHash) {
        cachedState = calculateGameState();
        stateNeedsUpdate = false;
        lastStateHash = board.hash;
    }
    return cachedState;
}

GameState Game::calculateGameState(){
    if (isFiftyMoveRule()) {
        return DRAW_50_MOVE;
    }
    if (isInsufficientMaterial()) {
        return DRAW_INSUFFICIENT_MATERIAL;
    }
    if (isThreefoldRepetition()) {
        return DRAW_REPETITION;
    }

    return checkForMateOrStaleMate();
}

bool Game::isPositionTerminal() {
    return !hasAnyLegalMove();
}


bool Game::isDrawByRule() {
    if (!drawStateValid || board.hash != lastDrawCheckHash) {
        cachedDrawState = (isFiftyMoveRule() || isThreefoldRepetition() || isInsufficientMaterial());
        drawStateValid = true;
        lastDrawCheckHash = board.hash;
    }
    return cachedDrawState;
}

void Game::invalidateGameState() {
    stateNeedsUpdate = true;
    drawStateValid = false;
}

void Game::clearHistory() {
    while (historyHead != nullptr) {
        HistoryNode* temp = historyHead;
        historyHead = historyHead->next;
        delete temp;
    }
    historyTail = nullptr;
    historySize = 0;
}

U64 Game::attackedBB(U8 enemyColour) {
    // Check if the square is attacked by any enemy piece
    U64 enemyPieces = board.getEnemyPieces();
    U64 friendlyKing = (enemyColour == nWhite) ? board.getBlackKing() : board.getWhiteKing();

    U64 allPieces = board.getAllPieces() & ~friendlyKing; // remove the friendly king so king does not block ray
    U64 attacked = 0ULL;

    U64 pieces = enemyPieces;
    while (pieces) {
        int square = __builtin_ctzll(pieces);
        pieces &= pieces - 1;
        enumPiece type = board.getPieceType(square);

        switch (type) {
            case nPawns:
                attacked |= tables.pawnMovesCapturesBB[enemyColour][square];
                break;
            case nKnights:
                attacked |= tables.knightBB[square];
                break;
            case nBishops:
                attacked |= getBishopAttacks(allPieces, square);
                break;
            case nRooks:
                attacked |= getRookAttacks(allPieces, square);
                break;
            case nQueens:
                attacked |= getBishopAttacks(allPieces, square);
                attacked |= getRookAttacks(allPieces, square);
                break;
            case nKings:
                attacked |= tables.kingBB[square];
                break;
            default:
                break;
        }
    }
    return attacked;
}





bool Game::isInCheck() {
    return isInCheck(board.friendlyColour());
}

bool Game::isInCheck(U8 colour) {
    // Find the king's bitboard for the given colour
    int kingSquare = __builtin_ctzll(colour == nWhite ? board.getWhiteKing() : board.getBlackKing());
    U8 enemyColour = (colour == nWhite) ? nBlack : nWhite;
    return isSquareAttacked(kingSquare, enemyColour);  // Only check one square
}

U64 Game::getRookAttacks(U64 occupancy, int square) {
    return MagicBitboard::instance().rookAttacks(square, occupancy);
}

U64 Game::getBishopAttacks(U64 occupancy, int square) {
    return MagicBitboard::instance().bishopAttacks(square, occupancy);
}

bool Game::isSquareAttacked(int square, U8 enemyColour) {
    U64 occupied = board.getAllPieces(); 
    
    U64 knightAttacks = tables.knightBB[square];
    if (knightAttacks & (enemyColour == nWhite ? board.getWhiteKnights() : board.getBlackKnights())) {
        return true;
    }
    
    U64 kingAttacks = tables.kingBB[square];
    if (kingAttacks & (enemyColour == nWhite ? board.getWhiteKing() : board.getBlackKing())) {
        return true;
    }
    
    U64 bishopAttacks = MagicBitboard::instance().bishopAttacks(square, occupied);
    U64 bishopsQueens = (enemyColour == nWhite ? board.getWhiteBishops() : board.getBlackBishops()) |
                        (enemyColour == nWhite ? board.getWhiteQueens() : board.getBlackQueens());
    if (bishopAttacks & bishopsQueens) {
        return true;
    }
    
    U64 rookAttacks = MagicBitboard::instance().rookAttacks(square, occupied);
    U64 rooksQueens = (enemyColour == nWhite ? board.getWhiteRooks() : board.getBlackRooks()) |
                      (enemyColour == nWhite ? board.getWhiteQueens() : board.getBlackQueens());
    if (rookAttacks & rooksQueens) {
        return true;
    }

    int row = square / 8;
    int col = square % 8;
    U64 enemyPawns = (enemyColour == nWhite ? board.getWhitePawns() : board.getBlackPawns());
    
    if (enemyColour == nWhite) {
        // White pawns attack from below (row-1)
        if (row > 0) {
            if (col > 0 && (enemyPawns & (1ULL << ((row-1)*8 + (col-1))))) return true;
            if (col < 7 && (enemyPawns & (1ULL << ((row-1)*8 + (col+1))))) return true;
        }
    } else {
        // Black pawns attack from above (row+1)
        if (row < 7) {
            if (col > 0 && (enemyPawns & (1ULL << ((row+1)*8 + (col-1))))) return true;
            if (col < 7 && (enemyPawns & (1ULL << ((row+1)*8 + (col+1))))) return true;
        }
    }

    return false;
}

bool Game::isLegal(const U8 from, const U8 to) {
    // Basic validation
    enumPiece pieceType = board.getPieceType(from);
    if (pieceType == nEmpty) return false;
    if (board.getColourType(from) != board.friendlyColour()) return false;


    MovesStruct legalMoves;
    int kingSquare = __builtin_ctzll( (board.friendlyColour() == nWhite) ? board.getWhiteKing() : board.getBlackKing());
    U64 enemyAttacks = attackedBB(board.enemyColour());
    U64 friendlyPieces = board.getFriendlyPieces();
    
    generateLegalMovesForPiece(pieceType, from, legalMoves, friendlyPieces, enemyAttacks, kingSquare, isInCheck()); // only called while in check

    // Check if 'to' square is in the generated pseudo-legal moves
    for (int i = 0; i < legalMoves.count; i++) {
        Move move = legalMoves.moveList[i];
        if (getTo(move) == to) return true;
    }

    return false;
}

GameState Game::checkForMateOrStaleMate() {
    bool isCheck = isInCheck();

    if (hasAnyLegalMove()) {
        return ONGOING;
    }
    return isCheck ? CHECKMATE : STALEMATE;
}

bool Game::isFiftyMoveRule() const {
    // Check if the halfmove clock is 100 or more
    return ((board.gameInfo & MOVE_MASK) >> 6) >= 100;
}

bool Game::isThreefoldRepetition() const {
    U64 currentHash = board.getHash();
    int count = 1; // include current position

    // only need to check moves before a capture or pawn move,
    // thus the number of positions needed to search is the half-move clock
    int numPositions = board.getHalfMoveClock();

    if (numPositions < 2) {
        return false; // not enough positions to check for repetition
    }

    // If both storage mechanisms may contain the same most-recent position (common when
    // seeding the stack from an existing linked-list history), avoid double-counting that one entry.
    bool skipStackBaseIfDuplicate = false;
    if (useStackHistory && historyTail != nullptr && searchDepth > 0) {
        if (searchHistory[0].hash == historyTail->state.hash) {
            skipStackBaseIfDuplicate = true;
        }
        if (--numPositions <= 0) return false; 

    }

    // Scan stack history (newest -> older)
    if (useStackHistory) {
        int minIndex = skipStackBaseIfDuplicate ? 1 : 0; // skip index 0 if it's the same as linked-list tail
        for (int i = searchDepth - 1; i >= minIndex; --i) {
            if (searchHistory[i].hash == currentHash) {
                if (++count >= 3) return true;
            }
            if (--numPositions <= 0) return false; 
        }
        // continue to scan linked-list too (older entries may live there)
    }

    // Scan linked-list history (most recent -> oldest)
    for (HistoryNode* node = historyTail; node != nullptr; node = node->prev) {
        if (node->state.hash == currentHash) {
            if (++count >= 3) return true;
        }
        if (--numPositions <= 0) return false; 
    }

    return false;
}

U64 Game::getPinnedPieces(U8 colour){
    if (cachedPinnedPieces != 0ULL) return cachedPinnedPieces;

    U8 enemyColour = (colour == nWhite) ? nBlack : nWhite;
    // Early exit: if no enemy sliding pieces, no pins possible
    U64 enemySliding = (enemyColour == nWhite) ?
        (board.getWhiteBishops() | board.getWhiteRooks() | board.getWhiteQueens()) :
        (board.getBlackBishops() | board.getBlackRooks() | board.getBlackQueens());
    
    if (!enemySliding) {
        cachedPinnedPieces = 0ULL;  // Explicitly set to 0
        return 0ULL;
    }

    U64 pinnedPieces = 0ULL;

    // get king square
    U64 kingBB = (colour == nWhite) ? board.getWhiteKing() : board.getBlackKing();
    if (!kingBB) return 0ULL;
    int kingSquare = __builtin_ctzll(kingBB);
    int kingRow = kingSquare / 8;
    int kingCol = kingSquare % 8;

    U64 occupied = board.getAllPieces();
    U64 friendlyPieces = board.getFriendlyPieces();

    U64 enemyBishopsQueens = (enemyColour == nWhite) ? 
        board.getWhiteBishops() | board.getWhiteQueens() : 
        board.getBlackBishops() | board.getBlackQueens();

    while (enemyBishopsQueens) {
        int enemySquare = __builtin_ctzll(enemyBishopsQueens);
        enemyBishopsQueens &= enemyBishopsQueens - 1;

        int enemyRow = enemySquare / 8;
        int enemyCol = enemySquare % 8;

        // only consider diagonals
        if (abs(enemyRow - kingRow) != abs(enemyCol - kingCol)) continue;

        // ray from king to enemy piece
        U64 ray = tables.rays[kingSquare][enemySquare];
        if (!ray) continue;

        // exclude king square and enemysquare to get all squares in between
        U64 interior = ray & ~((1ULL << kingSquare) | (1ULL << enemySquare));   
        U64 occupiedInterior = interior & occupied;

        if (__builtin_popcountll(occupiedInterior) != 1) continue; // No Pin!

        if (occupiedInterior & friendlyPieces) {
            pinnedPieces |= (occupiedInterior & friendlyPieces);
        }
    }

    U64 enemyRooksQueens = (enemyColour == nWhite) ? 
        board.getWhiteRooks() | board.getWhiteQueens() : 
        board.getBlackRooks() | board.getBlackQueens();

    while (enemyRooksQueens) {
        int enemySquare = __builtin_ctzll(enemyRooksQueens);
        enemyRooksQueens &= enemyRooksQueens - 1;

        int enemyRow = enemySquare / 8;
        int enemyCol = enemySquare % 8;

        // only consider straight lines
        if (!((enemyCol - kingCol == 0) || (enemyRow - kingRow == 0))) continue;

        // ray from king to enemy piece
        U64 ray = tables.rays[kingSquare][enemySquare];
        if (!ray) continue;

        // exclude king square and enemysquare to get all squares in between
        U64 interior = ray & ~((1ULL << kingSquare) | (1ULL << enemySquare));   
        U64 occupiedInterior = interior & occupied;

        if (__builtin_popcountll(occupiedInterior) != 1) continue; // No Pin!

        if (occupiedInterior & friendlyPieces) {
            pinnedPieces |= (occupiedInterior & friendlyPieces);
        }
    }

    cachedPinnedPieces = pinnedPieces;
    return pinnedPieces;
}

U64 Game::getPinnedMask(int square, U8 colour){
    
    U64 pinnedPieces = getPinnedPieces(colour);
    if (!(pinnedPieces & (1ULL << square))) {
        // if not pinned, can move anywhere
        return 0xFFFFFFFFFFFFFFFFULL;
    }

    // check if already cached
    if (cachedPinnedMasks[square] != 0ULL) return cachedPinnedMasks[square];

    // get enemy colour
    U8 enemyColour = (colour == nWhite) ? nBlack : nWhite;

    // get king square
    U64 kingBB = (colour == nWhite) ? board.getWhiteKing() : board.getBlackKing();
    if (!kingBB) return 0xFFFFFFFFFFFFFFFFULL; // should always be a king on the board
    int kingSquare = __builtin_ctzll(kingBB);

    U64 pinMask = 0ULL;

    // check enemy bishops and queens
    U64 enemyBishopsQueens = (enemyColour == nWhite) ?
        (board.getWhiteBishops() | board.getWhiteQueens()) :
        (board.getBlackBishops() | board.getBlackQueens());
    
    while (enemyBishopsQueens) {
        int enemySquare = __builtin_ctzll(enemyBishopsQueens);
        enemyBishopsQueens &= enemyBishopsQueens - 1;
        
        // use precomputed ray
        U64 ray = tables.rays[kingSquare][enemySquare];

        // if pinned square is on this ray
        if (ray & (1ULL << square)) {
            pinMask = ray;
            break;
        }
    }

    if (!pinMask) {
        U64 enemyRooksQueens = (enemyColour == nWhite) ?
            (board.getWhiteRooks() | board.getWhiteQueens()) :
            (board.getBlackRooks() | board.getBlackQueens());
        
        while (enemyRooksQueens) {
            int enemySquare = __builtin_ctzll(enemyRooksQueens);
            enemyRooksQueens &= enemyRooksQueens - 1;

            // use precomputed ray
            U64 ray = tables.rays[kingSquare][enemySquare];

            // if pinned square is on this ray
            if (ray & (1ULL << square)) {
                pinMask = ray;
                break;
            }
        }
    }

    cachedPinnedMasks[square] = pinMask;
    return pinMask;
}


MovesStruct Game::generateAllLegalMoves(bool isCaptureOnly) {    
    MovesStruct legalMoves;

    inMoveGeneration = true;

    U8 colour = board.friendlyColour();
    U8 enemyColour = board.enemyColour();

    // computes all pinned pieces 
    currentPinnedPieces = getPinnedPieces(colour);
    U64 enemyAttacks = attackedBB(enemyColour);

    int kingSquare = __builtin_ctzll(colour == nWhite ? board.getWhiteKing() : board.getBlackKing());

    bool inCheck = enemyAttacks & (1ULL << kingSquare);

    if (inCheck){
        checkers = getCheckers(colour, kingSquare);
    }

    U64 pieces = board.getFriendlyPieces(); // get all friendly pieces

    U64 friendlyPieces = pieces;

    while (pieces) {
        int square = __builtin_ctzll(pieces);  // get the least significant set bit
        pieces &= pieces - 1;                  // remove the least significant set bit

        enumPiece pieceType = board.getPieceType(square);
        if (pieceType != nEmpty) {
            // generateMoves(pieceType, square, pseudoMoves, isCaptureOnly);
            generateLegalMovesForPiece(pieceType, square, legalMoves, friendlyPieces, enemyAttacks, kingSquare, inCheck, isCaptureOnly);
        }
    }

    currentPinnedPieces = 0ULL;  // reset pinned pieces for next search
    checkers = 0ULL;  // reset pinned pieces for next search

    inMoveGeneration = false;
    return legalMoves;
};

void Game::generateLegalMovesForPiece(
    enumPiece pieceType, int square, MovesStruct& legalMoves, U64& friendlyPieces, U64& enemyAttacks, int kingSquare, bool inCheck, bool isCaptureOnly
){   
    switch (pieceType) {
        case nKings: generateKingMovesForSquare(square, legalMoves, friendlyPieces, enemyAttacks, inCheck, isCaptureOnly); break;
        case nKnights: generateKnightMovesForSquare(square, legalMoves, friendlyPieces, kingSquare, inCheck, isCaptureOnly); break;
        case nBishops: generateBishopMovesForSquare(square, legalMoves, friendlyPieces, kingSquare, inCheck, isCaptureOnly); break;
        case nRooks: generateRookMovesForSquare(square, legalMoves, friendlyPieces, kingSquare, inCheck, isCaptureOnly); break;
        case nQueens: generateQueenMovesForSquare(square, legalMoves, friendlyPieces, kingSquare, inCheck, isCaptureOnly); break;
        case nPawns: generatePawnMovesForSquare(square, legalMoves, kingSquare, inCheck, isCaptureOnly); break;
        default: break;
    }
};

void Game::generateKingMovesForSquare(int square, MovesStruct& legalMoves, U64& friendlyPieces,  U64& enemyAttacks, bool inCheck, bool isCaptureOnly) {

    U64 movesBB = tables.kingBB[square] & ~enemyAttacks & ~friendlyPieces; // filter friendly pieces and enemy attacks

    if (isCaptureOnly){
        U64 captureMoves = movesBB & board.getEnemyPieces();
        addMovesToStructFast(nKings, legalMoves, square, captureMoves); // if capture only, filter to enemy pieces
        return;
    }
    
    if (inCheck){
        addMovesToStructFast(nKings, legalMoves, square, movesBB);
        return;
    }

    U64 occupied = board.getAllPieces(); // all occupied squares
    
    // White king on e1 (square 4)
    if (board.friendlyColour() == nWhite && square == 4) {
        // Kingside castle (e1-g1)
        if ((board.gameInfo & WK_CASTLE)) {
            U64 kingSideSquares = (1ULL << 5) | (1ULL << 6); // f1, g1
            U64 kingSideCheck = (1ULL << 4) | (1ULL << 5) | (1ULL << 6); // e1, f1, g1
            
            // Check: squares between king and rook are empty AND not attacked
            if (!(occupied & kingSideSquares) && !(enemyAttacks & kingSideCheck)) {
                movesBB |= (1ULL << 6); // Add g1 as valid move
            }
        }
        
        // Queenside castle (e1-c1)
        if ((board.gameInfo & WQ_CASTLE)) {
            U64 queenSideEmpty = (1ULL << 1) | (1ULL << 2) | (1ULL << 3); // b1, c1, d1
            U64 queenSideCheck = (1ULL << 2) | (1ULL << 3) | (1ULL << 4); // c1, d1, e1
            
            // Check: squares between king and rook are empty AND king path not attacked
            if (!(occupied & queenSideEmpty) && !(enemyAttacks & queenSideCheck)) {
                movesBB |= (1ULL << 2); // Add c1 as valid move
            }
        }
    }

    // Black king on e8 (square 60)
    else if (board.friendlyColour() == nBlack && square == 60) {
        // Kingside castle (e8-g8)
        if ((board.gameInfo & BK_CASTLE)) {
            U64 kingSideSquares = (1ULL << 61) | (1ULL << 62); // f8, g8
            U64 kingSideCheck = (1ULL << 60) | (1ULL << 61) | (1ULL << 62); // e8, f8, g8
            
            if (!(occupied & kingSideSquares) && !(enemyAttacks & kingSideCheck)) {
                movesBB |= (1ULL << 62); // Add g8 as valid move
            }
        }
        
        // Queenside castle (e8-c8)
        if ((board.gameInfo & BQ_CASTLE)) {
            U64 queenSideEmpty = (1ULL << 57) | (1ULL << 58) | (1ULL << 59); // b8, c8, d8
            U64 queenSideCheck = (1ULL << 58) | (1ULL << 59) | (1ULL << 60); // c8, d8, e8
            
            if (!(occupied & queenSideEmpty) && !(enemyAttacks & queenSideCheck)) {
                movesBB |= (1ULL << 58); // Add c8 as valid move
            }
        }
    }
    addMovesToStructFast(nKings, legalMoves, square, movesBB);
}

void Game::generateQueenMovesForSquare(int square, MovesStruct& legalMoves, U64& friendlyPieces, int kingSquare, bool inCheck, bool isCaptureOnly) {
    U64 movesBB = (getBishopAttacks(board.getAllPieces(), square) | getRookAttacks(board.getAllPieces(), square)) & ~friendlyPieces;
    // check if this piece is pinned
    if (currentPinnedPieces & (1ULL << square)){
        U64 pinnedMask = getPinnedMask(square, board.friendlyColour());
        movesBB &= pinnedMask;
    }

    if (isCaptureOnly){
        movesBB &= board.getEnemyPieces(); // if capture only, filter to enemy pieces
    }

    // if not in check, not need to validate
    // pins are already accouted for
    if (!inCheck){
        addMovesToStructFast(nQueens, legalMoves, square, movesBB);
        return;
    }

    // expensive case
    addMovesToStructInCheck(nQueens, legalMoves, kingSquare, square, movesBB);
}

void Game::generateRookMovesForSquare(int square, MovesStruct& legalMoves, U64& friendlyPieces, int kingSquare, bool inCheck, bool isCaptureOnly) {
    U64 movesBB = getRookAttacks(board.getAllPieces(), square) & ~friendlyPieces;
    // check if this piece is pinned
    if (currentPinnedPieces & (1ULL << square)){
        U64 pinnedMask = getPinnedMask(square, board.friendlyColour());
        movesBB &= pinnedMask;
    }

    if (isCaptureOnly){
        movesBB &= board.getEnemyPieces(); // if capture only, filter to enemy pieces
    }

    // if not in check, not need to validate
    // pins are already accouted for
    if (!inCheck){
        addMovesToStructFast(nRooks, legalMoves, square, movesBB);
        return;
    }

    // expensive case
    addMovesToStructInCheck(nRooks, legalMoves, kingSquare, square, movesBB);
}


void Game::generateBishopMovesForSquare(int square, MovesStruct& legalMoves, U64& friendlyPieces, int kingSquare, bool inCheck, bool isCaptureOnly) {
    U64 movesBB = getBishopAttacks(board.getAllPieces(), square) & ~friendlyPieces;
    // check if this piece is pinned
    if (currentPinnedPieces & (1ULL << square)){
        U64 pinnedMask = getPinnedMask(square, board.friendlyColour());
        movesBB &= pinnedMask;
    }

    if (isCaptureOnly){
        movesBB &= board.getEnemyPieces(); // if capture only, filter to enemy pieces
    }

    // if not in check, not need to validate
    // pins are already accouted for
    if (!inCheck){
        addMovesToStructFast(nBishops, legalMoves, square, movesBB);
        return;
    }

    // expensive case
    addMovesToStructInCheck(nBishops, legalMoves, kingSquare, square, movesBB);
}


void Game::generateKnightMovesForSquare(int square, MovesStruct& legalMoves, U64& friendlyPieces, int kingSquare, bool inCheck, bool isCaptureOnly) {
    U64 movesBB = tables.knightBB[square] & ~friendlyPieces; // filter out friendlys

    // check if this piece is pinned
    if (currentPinnedPieces & (1ULL << square)){
        U64 pinnedMask = getPinnedMask(square, board.friendlyColour());
        movesBB &= pinnedMask;
    }

    if (isCaptureOnly){
        movesBB &= board.getEnemyPieces(); // if capture only, filter to enemy pieces
    }

    // if not in check, not need to validate
    // pins are already accouted for
    if (!inCheck){
        addMovesToStructFast(nKnights, legalMoves, square, movesBB);
        return;
    }

    // expensive case
    addMovesToStructInCheck(nKnights, legalMoves, kingSquare, square, movesBB);
}

void Game::generatePawnMovesForSquare(int square, MovesStruct& legalMoves, int kingSquare, bool inCheck, bool isCaptureOnly) {
    
    U64 movesBB = 0ULL;
    int row = square / 8;
    int col = square % 8;

    // start with captures
    movesBB |= tables.pawnMovesCapturesBB[board.friendlyColour()][square] & board.getEnemyPieces();

    int epSquare = board.getEnPassantSquare(); // get en passant square if available
    if (epSquare != -1) {
        int epRow = epSquare / 8;
        int epCol = epSquare % 8;
        if (board.friendlyColour() == nWhite && row == 4 && abs(col - epCol) == 1 && epRow == 5)
            movesBB |= (1ULL << epSquare);
        if (board.friendlyColour() == nBlack && row == 3 && abs(col - epCol) == 1 && epRow == 2)
            movesBB |= (1ULL << epSquare);
    }

    // return early to only check for captures
    if (isCaptureOnly)  {
        if (currentPinnedPieces & (1ULL << square)){
            U64 pinnedMask = getPinnedMask(square, board.friendlyColour());
            movesBB &= pinnedMask;
        }

        if (!inCheck){
            addPawnMovesToStructFast(legalMoves, square, movesBB);
            return;
        }
        addMovesToStructInCheck(nPawns, legalMoves, kingSquare, square, movesBB);
        return; // if only captures, return early
    }

    U64 empty = ~(board.getAllPieces()); // all empty squares
    movesBB |= tables.pawnMovesBB[board.friendlyColour()][square] & empty;

    // Double pawn push
    if (board.friendlyColour() == nWhite && row == 1 && !(movesBB & (1ULL << (square + 8)))) {
        movesBB &= ~(1ULL << (square + 16)); 
    } else if (board.friendlyColour() == nBlack && row == 6 && !(movesBB & (1ULL << (square - 8)))) {
        movesBB &= ~(1ULL << (square - 16)); // remove double pawn push if square is not empty
    }

    if (currentPinnedPieces & (1ULL << square)){
        U64 pinnedMask = getPinnedMask(square, board.friendlyColour());
        movesBB &= pinnedMask;
    }

    if (!inCheck){
        addPawnMovesToStructFast(legalMoves, square, movesBB);
        return;
    }
    addMovesToStructInCheck(nPawns, legalMoves, kingSquare, square, movesBB);
}

void Game::addMovesToStructFast(enumPiece pieceType, MovesStruct& legalMoves, int square, U64& movesBB) {

    int epSquare = board.getEnPassantSquare();
    
    while(movesBB) {
        int to = __builtin_ctzll(movesBB);
        movesBB &= movesBB - 1;
        Move move = makeMove(square, to, epSquare, pieceType, board.getPieceType(to));
        legalMoves.addMove(move);
    }
}

void Game::addPawnMovesToStructFast(MovesStruct& legalMoves, int square, U64& movesBB) {

    // remove moves that land on friendly pieces
    movesBB &= ~board.getFriendlyPieces();
    
    int epSquare = board.getEnPassantSquare();
    enumPiece pieceType = board.getPieceType(square);

    U64 promotionSquares = movesBB & (0xFFULL | 0xFF00000000000000ULL); // Ranks 1 and 8

    while(movesBB) {
        int to = __builtin_ctzll(movesBB);
        movesBB &= movesBB - 1;

        enumPiece capturedPiece = board.getPieceType(to);

        // promotion check
        if (promotionSquares & (1ULL << to)) {
            Move m1 = makeMove(square, to, epSquare, pieceType, capturedPiece, nKnights);
            Move m2 = makeMove(square, to, epSquare, pieceType, capturedPiece, nBishops);
            Move m3 = makeMove(square, to, epSquare, pieceType, capturedPiece, nRooks);
            Move m4 = makeMove(square, to, epSquare, pieceType, capturedPiece, nQueens);
            legalMoves.addMove(m1);
            legalMoves.addMove(m2);
            legalMoves.addMove(m3);   
            legalMoves.addMove(m4);
        } else {
            Move move = makeMove(square, to, epSquare, pieceType, capturedPiece);
            legalMoves.addMove(move);
        }
    }
}


void Game::addMovesToStructInCheck(enumPiece pieceType, MovesStruct& moves, int kingSquare, int square, U64& movesBB) {
    
    int numCheckers = __builtin_popcountll(checkers);
    
    // only king moves are legal if in double check and they are already account for
    if (numCheckers >= 2){
        if (pieceType != nKings) return;

        // if pieceType is king, this addMovesToStructInCheck should not be called but handle anyway
        addMovesToStructFast(nKings, moves, square, movesBB);
        return;
    }

    if (numCheckers == 1){
        
        int checkerSquare = __builtin_ctzll(checkers);
        enumPiece checkerPiece = board.getPieceType(checkerSquare);
        

        // already filtered but add anyway
        if (pieceType == nKings) {
            addMovesToStructFast(pieceType, moves, square, movesBB);
            return;
        }

        // non-king pieces must capture checker or block ray
        U64 legalSquares = (1ULL << checkerSquare);
        
        // if checker is a sliding piece, can also block the ray
        if (checkerPiece == nBishops || checkerPiece == nRooks || checkerPiece == nQueens) {

            U64 ray = tables.rays[kingSquare][checkerSquare];
            if (ray) {
                U64 blockingSquares = ray & ~(1ULL << checkerSquare);
                legalSquares |= blockingSquares;
            }
        }
        
        // filer moves to only legal squares
        movesBB &= legalSquares;
        
        if (pieceType == nPawns){
            addPawnMovesToStructFast(moves, square, movesBB);
        } else{
            addMovesToStructFast(pieceType, moves, square, movesBB);
        }
        return;
    }
    
    // numCheckers == 0, shouldnt happen but hanlde anyway
    if (pieceType == nPawns) {
        addPawnMovesToStructFast(moves, square, movesBB);
    } else {
        addMovesToStructFast(pieceType, moves, square, movesBB);
    }

}

U64 Game::getCheckers(U8 colour, int kingSquare) {
    U64 checkers = 0ULL;
    U8 enemyColour = (colour == nWhite) ? nBlack : nWhite;
    
    // find which enemy pieces attack the king
    U64 occupied = board.getAllPieces();
    
    // check knights
    U64 knightAttacks = tables.knightBB[kingSquare];
    U64 enemyKnights = (enemyColour == nWhite) ? board.getWhiteKnights() : board.getBlackKnights();
    checkers |= knightAttacks & enemyKnights;
    
    // Check pawns
    int row = kingSquare / 8;
    int col = kingSquare % 8;
    U64 enemyPawns = (enemyColour == nWhite) ? board.getWhitePawns() : board.getBlackPawns();
    if (enemyColour == nWhite) {
        if (row > 0) {
            if (col > 0 && (enemyPawns & (1ULL << ((row-1)*8 + (col-1))))) 
                checkers |= (1ULL << ((row-1)*8 + (col-1)));
            if (col < 7 && (enemyPawns & (1ULL << ((row-1)*8 + (col+1))))) 
                checkers |= (1ULL << ((row-1)*8 + (col+1)));
        }
    } else {
        if (row < 7) {
            if (col > 0 && (enemyPawns & (1ULL << ((row+1)*8 + (col-1))))) 
                checkers |= (1ULL << ((row+1)*8 + (col-1)));
            if (col < 7 && (enemyPawns & (1ULL << ((row+1)*8 + (col+1))))) 
                checkers |= (1ULL << ((row+1)*8 + (col+1)));
        }
    }
    
    // check sliding pieces (bishops, rooks, queens)
    U64 bishopAttacks = getBishopAttacks(occupied, kingSquare);
    U64 rookAttacks = getRookAttacks(occupied, kingSquare);
    
    U64 enemyBishopsQueens = (enemyColour == nWhite) ? 
        (board.getWhiteBishops() | board.getWhiteQueens()) :
        (board.getBlackBishops() | board.getBlackQueens());
    checkers |= bishopAttacks & enemyBishopsQueens;
    
    U64 enemyRooksQueens = (enemyColour == nWhite) ?
        (board.getWhiteRooks() | board.getWhiteQueens()) :
        (board.getBlackRooks() | board.getBlackQueens());
    checkers |= rookAttacks & enemyRooksQueens;
    
    // check king (shouldn't happen, but be safe)
    U64 kingAttacks = tables.kingBB[kingSquare];
    U64 enemyKing = (enemyColour == nWhite) ? board.getWhiteKing() : board.getBlackKing();
    checkers |= kingAttacks & enemyKing;
    
    return checkers;
}


void Game::pushMove(Move move) {

    // move data - compute piece type before we modify the board
    const U8 from = getFrom(move); // extract from square, mask to 6 bits
    const U8 to = getTo(move); // extract to square, mask to 6 bits
    const enumPiece piece = board.getPieceType(from);

    // invalidate cached pinned pieces and masks
    cachedPinnedPieces = 0ULL;
    memset(cachedPinnedMasks, 0ULL, sizeof(cachedPinnedMasks));

    if (useStackHistory) {
        searchHistory[searchDepth].move = move;
        searchHistory[searchDepth].gameInfo = board.gameInfo;
        searchHistory[searchDepth].hash = board.hash;
        searchHistory[searchDepth].pieceMoved = piece; // Store piece type for efficient unmake
        
        searchDepth++;

    } else {
        // save current state
        BoardState currentState;   
        currentState.move = move;
        currentState.gameInfo = board.gameInfo;
        currentState.hash = board.hash;
        currentState.pieceMoved = piece; // Store piece type for efficient unmake
        pushBoardState(currentState);
    }
    const enumPiece colour = board.getColourType(from);
    const enumPiece capturedPiece = getCapturedPiece(move);;
    const enumPiece capturedColour = colour == nWhite ? nBlack : nWhite;
    const moveType moveType = getMoveType(move);

    // update zorbist hash for castling rights 
    U16 oldGameInfo = board.gameInfo;
    int oldCastlingIdx = board.getCastlingIndex();
    int oldEpFile = (oldGameInfo & EP_IS_SET) ? ((oldGameInfo & EP_FILE_MASK) >> EP_FILE_SHIFT) : -1;

    board.removePiece(from, piece, colour); // remove the piece from the "from" square

    // capture logic
    if (isCapture(move)) {
        if (isEPCapture(move)) {
            int capturePawnSquare = (colour == nWhite) ? to - 8 : to + 8; // calculate the square of the captured pawn
            board.removePiece(capturePawnSquare, nPawns, capturedColour); // remove the captured pawn
        } else {
            board.removePiece(to, capturedPiece, capturedColour); // remove the captured piece
            
            // update castling rights if a rook is caputured
            if (to == 0) board.gameInfo &= ~WQ_CASTLE;       // a1 rook captured
            else if (to == 7) board.gameInfo &= ~WK_CASTLE;  // h1 rook captured
            else if (to == 56) board.gameInfo &= ~BQ_CASTLE; // a8 rook captured
            else if (to == 63) board.gameInfo &= ~BK_CASTLE; // h8 rook captured
        }
    }

    board.clearEpSquare(); // clear the en passant square before applying the move

    enumPiece finalPiece = piece; // default to the moved piece
    if (isPromoCapture(move) || isPromotion(move)) {
        // handle promotion
        finalPiece = getPromotionPiece(move); // default to queen promotion
        board.setPiece(to, finalPiece, colour); // set the promoted piece
    } else {
        // for all other moves, just set the piece to the "to" square
        board.setPiece(to, piece, colour);

        if (piece == nPawns) {
            if (abs((int)from - (int)to) == 16) {
                int epSquare = (from + to) / 2;
                board.setEpSquare(epSquare);
            }
        } 
    }

    board.updateCastlePieces(moveType, colour); // update castling rights if necessary

    // update halfmove clock
    if (isCapture(move) || piece == nPawns){
       board.gameInfo &= ~MOVE_MASK; // reset halfmove clock
    } else {
        board.gameInfo = (board.gameInfo & ~MOVE_MASK) | (((((board.gameInfo & MOVE_MASK) >> 6) + 1) << 6) & MOVE_MASK);
    }

    board.updateCasltingRights(piece, colour, from); // update castling rights

    board.gameInfo ^= TURN_MASK;

    // get new state for hash calculation
    int newCastlingIdx = board.getCastlingIndex();
    int newEpFile = (board.gameInfo & EP_IS_SET) ? ((board.gameInfo & EP_FILE_MASK) >> EP_FILE_SHIFT) : -1;

     // ONE SINGLE HASH UPDATE with all changes
    board.hash ^= tables.zobristTable[board.getPieceIndex(piece, colour)][from];           // Remove old piece
    board.hash ^= tables.zobristTable[board.getPieceIndex(finalPiece, colour)][to];       // Add new piece

    // captures
    if (isCapture(move)) {
        if (isEPCapture(move)) {
            int capturePawnSquare = (colour == nWhite) ? to - 8 : to + 8;
            board.hash ^= tables.zobristTable[board.getPieceIndex(nPawns, capturedColour)][capturePawnSquare];
        } else {
            board.hash ^= tables.zobristTable[board.getPieceIndex(capturedPiece, capturedColour)][to];
        }
    }

    // castling rook hash updates
    if (moveType == KING_CASTLE) {
        if (colour == nWhite) {
            board.hash ^= tables.zobristTable[board.getPieceIndex(nRooks, nWhite)][7];
            board.hash ^= tables.zobristTable[board.getPieceIndex(nRooks, nWhite)][5];
        } else {
            board.hash ^= tables.zobristTable[board.getPieceIndex(nRooks, nBlack)][63];
            board.hash ^= tables.zobristTable[board.getPieceIndex(nRooks, nBlack)][61];
        }
    } else if (moveType == QUEEN_CASTLE) {
        if (colour == nWhite) {
            board.hash ^= tables.zobristTable[board.getPieceIndex(nRooks, nWhite)][0];
            board.hash ^= tables.zobristTable[board.getPieceIndex(nRooks, nWhite)][3];
        } else {
            board.hash ^= tables.zobristTable[board.getPieceIndex(nRooks, nBlack)][56];
            board.hash ^= tables.zobristTable[board.getPieceIndex(nRooks, nBlack)][59];
        }
    }

    // Update castling hash
    board.hash ^= tables.zobristCastling[oldCastlingIdx];
    board.hash ^= tables.zobristCastling[newCastlingIdx];

    // Update en passant hash
    if (oldEpFile != -1) {
        board.hash ^= tables.zobristEnPassant[oldEpFile];
    }
    if (newEpFile != -1) {
        board.hash ^= tables.zobristEnPassant[newEpFile];
    }
    board.hash ^= tables.zobristSideToMove; // update hash for side to move

    invalidateGameState();
    
    // board.calculateHash(); // recalculate the hash for the board state
}



void Game::popMove() {

    BoardState prevState;

    // invalidate cached pinned pieces and masks
    cachedPinnedPieces = 0ULL;
    memset(cachedPinnedMasks, 0ULL, sizeof(cachedPinnedMasks));

    if (useStackHistory) {
        // stack --> fast move generation
        if (searchDepth == 0) {
            throw std::runtime_error("No moves to pop");
        }

        searchDepth--;
        prevState = searchHistory[searchDepth];

    } else {
        // linked list --> normal move generation
        if (historySize == 0) {
            throw std::runtime_error("No moves to pop");
        }

        prevState = popBoardState();
    }

    board.gameInfo = prevState.gameInfo;
    board.hash = prevState.hash; // restore the hash from the previous state
    Move move = prevState.move;
    enumPiece pieceMoved = prevState.pieceMoved; // Use stored piece type (original piece)

    if (move == MOVE_NONE) return; // safety checks

    int from = getFrom(move);
    int to = getTo(move);
    int flags = getFlags(move);
    enumPiece capturedPiece = getCapturedPiece(move);;

    // Determine what piece is currently on the board at 'to' to remove it
    // For normal moves, it's the same as pieceMoved.
    // For promotions, it's the promoted piece (e.g., Queen), not the original (Pawn).
    enumPiece pieceToRemove = pieceMoved;
    if (isPromotion(move) || isPromoCapture(move)) {
        pieceToRemove = getPromotionPiece(move);
    }

    enumPiece enemyColour = board.enemyColour();
    enumPiece friendlyColour = board.friendlyColour();
    int epSquare;

    board.removePiece(to, pieceToRemove, friendlyColour);

    switch (flags) {
        case QUIET_MOVES: 
            board.setPiece(from, pieceMoved, friendlyColour);
            break;
        case DOUBLE_PAWN_PUSH:
            board.setPiece(from, pieceMoved, friendlyColour);
            break;
        case CAPTURE:
            board.setPiece(to, capturedPiece, enemyColour);
            board.setPiece(from, pieceMoved, friendlyColour);
            break;
        case EP_CAPTURE:
            epSquare = (friendlyColour == nWhite) ? to - 8 : to + 8;
            board.setPiece(epSquare, nPawns, enemyColour);
            board.setPiece(from, nPawns, friendlyColour);
            break;
        case KNIGHT_PROMO:
            board.setPiece(from, nPawns, friendlyColour);
            break;
        case BISHOP_PROMO:
            board.setPiece(from, nPawns, friendlyColour);
            break;
        case ROOK_PROMO:
            board.setPiece(from, nPawns, friendlyColour);
            break;
        case QUEEN_PROMO:
            board.setPiece(from, nPawns, friendlyColour);
            break;
        case KNIGHT_PROMO_CAPTURE:
            board.setPiece(to, capturedPiece, enemyColour);
            board.setPiece(from, nPawns, friendlyColour);
            break;
        case BISHOP_PROMO_CAPTURE:
            board.setPiece(to, capturedPiece, enemyColour);
            board.setPiece(from, nPawns, friendlyColour);
            break;
        case ROOK_PROMO_CAPTURE:
            board.setPiece(to, capturedPiece, enemyColour);
            board.setPiece(from, nPawns, friendlyColour);
            break;
        case QUEEN_PROMO_CAPTURE:
            board.setPiece(to, capturedPiece, enemyColour);
            board.setPiece(from, nPawns, friendlyColour);
            break;
        case KING_CASTLE:
            board.setPiece((friendlyColour == nWhite) ? 4 : 60, nKings, friendlyColour); // e1 : e8
            board.removePiece((friendlyColour == nWhite) ? 5 : 61, nRooks, friendlyColour); // f1 : f8
            board.setPiece((friendlyColour == nWhite) ? 7 : 63, nRooks, friendlyColour); // h1 : h8
            break;
        case QUEEN_CASTLE:
            board.setPiece((friendlyColour == nWhite) ? 4 : 60, nKings, friendlyColour); // e1 : e8
            board.removePiece((friendlyColour == nWhite) ? 3 : 59, nRooks, friendlyColour); // d1 : d8
            board.setPiece((friendlyColour == nWhite) ? 0 : 56, nRooks, friendlyColour); // a1 : a8
            break;
        default:
            throw std::runtime_error("Unknown move type");
    }  

    invalidateGameState();
    // board.calculateHash(); // recalculate the hash for the board state

}

void Game::pushBoardState(const BoardState& state) {
    HistoryNode* newNode = new HistoryNode(state);
    
    if (!historyHead) {
        historyHead = historyTail = newNode;
    } else {
        historyTail->next = newNode;
        newNode->prev = historyTail;
        historyTail = newNode;
    }
    historySize++;
}

BoardState Game::popBoardState() {
    if (historyTail == nullptr) {
        throw std::runtime_error("Cannot pop from empty history");
    }

    HistoryNode* temp = historyTail;
    BoardState state = temp->state;

    if (historyTail == historyHead) {
        historyHead = historyTail = nullptr;
    } else {
        historyTail = historyTail->prev;
        historyTail->next = nullptr;
    }

    delete temp;
    historySize--;
    return state;
}