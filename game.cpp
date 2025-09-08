#include "game.h"

using namespace std;

void Game::pushMove(const Move& move) {

    if (useStackHistory) {
        searchHistory[searchDepth].move = move;
        searchHistory[searchDepth].gameInfo = board.gameInfo;
        searchHistory[searchDepth].hash = board.hash;
        
        searchDepth++;

    } else {
        // save current state
        BoardState currentState;   
        currentState.move = move;
        currentState.gameInfo = board.gameInfo;
        currentState.hash = board.hash; 
        pushBoardState(currentState);
    }

    // move data
    const U8 from = move.getFrom(); // extract from square, mask to 6 bits
    const U8 to = move.getTo(); // extract to square, mask to 6 bits
    const U16 moveInt = move.getMove(); // get the full move
    const enumPiece piece = board.getPieceType(from);
    const enumPiece colour = board.getColourType(from);
    const enumPiece capturedPiece = board.getPieceType(to);
    const enumPiece capturedColour = colour == nWhite ? nBlack : nWhite;
    const moveType moveType = move.getMoveType();

    // update zorbist hash for castling rights 
    U16 oldGameInfo = board.gameInfo;
    int oldCastlingIdx = board.getCastlingIndex();
    int oldEpFile = (oldGameInfo & EP_IS_SET) ? ((oldGameInfo & EP_FILE_MASK) >> EP_FILE_SHIFT) : -1;

    board.removePiece(from, piece, colour); // remove the piece from the "from" square

    // capture logic
    if (move.isCapture()) {
        if (move.isEPCapture()) {
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
    if (move.isPromoCapture() || move.isPromotion()) {
        // handle promotion
        finalPiece = move.getPromotionPiece(); // default to queen promotion
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

    board.updateCastlePieces(moveType, piece, colour); // update castling rights if necessary

    // update halfmove clock
    if (move.isCapture() || piece == nPawns){
       board.gameInfo &= ~MOVE_MASK; // reset halfmove clock
    } else {
        board.gameInfo = (board.gameInfo & ~MOVE_MASK) | (((((board.gameInfo & MOVE_MASK) >> 6) + 1) << 6) & MOVE_MASK);
    }

    board.updateCasltingRights(piece, colour, from); // update castling rights

    board.gameInfo ^= TURN_MASK;

    // get new state for hash calculation
    int newCastlingIdx = board.getCastlingIndex();
    int newEpFile = (board.gameInfo & EP_IS_SET) ? 
                    ((board.gameInfo & EP_FILE_MASK) >> EP_FILE_SHIFT) : -1;


     // ONE SINGLE HASH UPDATE with all changes
    board.hash ^= tables.zobristTable[board.getPieceIndex(piece, colour)][from];           // Remove old piece
    board.hash ^= tables.zobristTable[board.getPieceIndex(finalPiece, colour)][to];       // Add new piece

    // captures
    if (move.isCapture()) {
        if (move.isEPCapture()) {
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
void Game::enableFastMode() {
    useStackHistory = true;
    searchHistory[0].move = Move();                // empty/dummy move
    searchHistory[0].gameInfo = board.gameInfo;
    searchHistory[0].hash = board.getHash();
    searchDepth = 1;
}

void Game::disableFastMode() {
    useStackHistory = false;
}

void Game::popMove() {


    BoardState prevState;

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


    int from = move.getFrom();
    int to = move.getTo();
    int flags = move.getFlags();
    enumPiece capturedPiece = move.getCapturedPiece();
    enumPiece pieceMoved = board.getPieceType(to);

    enumPiece enemyColour = board.enemyColour();
    enumPiece friendlyColour = board.friendlyColour();
    int epSquare;

    board.removePiece(to, pieceMoved, friendlyColour);

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

GameState Game::checkForMateOrStaleMate() {
    bool isCheck = isInCheck();

    if (hasAnyLegalMove()) {
        return ONGOING;
    }
    return isCheck ? CHECKMATE : STALEMATE;
}

bool Game::hasAnyLegalMove() {
    bool oldInMoveGeneration = inMoveGeneration;
    inMoveGeneration = true; // prevent recursive calls to hasAnyLegalMove

    U64 pieces = board.getFriendlyPieces();

    while (pieces) {
        int square = __builtin_ctzll(pieces);  // get the least significant set bit
        pieces &= pieces - 1;                  // remove the least significant set bit

        enumPiece pieceType = board.getPieceType(square);
        if (pieceType != nEmpty) {
            if (hasLegalMoveFromSquare(pieceType, square)) {
                inMoveGeneration = oldInMoveGeneration;
                return true;
            }
        }
    }

    inMoveGeneration = oldInMoveGeneration;
    return false;
}


bool Game::hasLegalMoveFromSquare(enumPiece pieceType, int square) {
    MovesStruct moves;
    generateMoves(pieceType, square, moves);

    for (int i = 0; i < moves.count; i++) {
        Move move = moves.moveList[i];
        
        pushMove(move);
        bool legal = !isInCheck(board.enemyColour());
        popMove();
        
        if (legal) {
            return true;  // Found at least one legal move
        }
    }
    return false;
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

U64 Game::getRookAttacks(U64 occupancy, int square) {
    U64 attacks = 0ULL;
    int r = square / 8, c = square % 8;

    // up
    for (int i = r + 1; i < 8; ++i) {
        int sq = i * 8 + c;
        attacks |= 1ULL << sq;
        if (occupancy & (1ULL << sq)) break;
    }

    // down
    for (int i = r - 1; i >= 0; --i) {
        int sq = i * 8 + c;
        attacks |= 1ULL << sq;
        if (occupancy & (1ULL << sq)) break;
    }

    // right
    for (int i = c + 1; i < 8; ++i) {
        int sq = r * 8 + i;
        attacks |= 1ULL << sq;
        if (occupancy & (1ULL << sq)) break;
    }

    // left
    for (int i = c - 1; i >= 0; --i) {
        int sq = r * 8 + i;
        attacks |= 1ULL << sq;
        if (occupancy & (1ULL << sq)) break;
    }

    return attacks;
}

U64 Game::getBishopAttacks(U64 occupancy, int square) {
    U64 attacks = 0ULL;
    int r = square / 8, c = square % 8;

    // up-right
    for (int i = 1; r + i < 8 && c + i < 8; ++i) {
        int sq = (r + i) * 8 + (c + i);
        attacks |= 1ULL << sq;
        if (occupancy & (1ULL << sq)) break;
    }

    // up-left
    for (int i = 1; r + i < 8 && c - i >= 0; ++i) {
        int sq = (r + i) * 8 + (c - i);
        attacks |= 1ULL << sq;
        if (occupancy & (1ULL << sq)) break;
    }

    // down-right
    for (int i = 1; r - i >= 0 && c + i < 8; ++i) {
        int sq = (r - i) * 8 + (c + i);
        attacks |= 1ULL << sq;
        if (occupancy & (1ULL << sq)) break;
    }

    // down-left
    for (int i = 1; r - i >= 0 && c - i >= 0; ++i) {
        int sq = (r - i) * 8 + (c - i);
        attacks |= 1ULL << sq;
        if (occupancy & (1ULL << sq)) break;
    }

    return attacks;
}



MovesStruct Game::generateAllLegalMoves(bool isCaptureOnly) {    

    MovesStruct pseudoMoves;
    MovesStruct legalMoves;

    inMoveGeneration = true;

    U64 pieces = board.getFriendlyPieces(); // get all friendly pieces

    while (pieces) {
        int square = __builtin_ctzll(pieces);  // get the least significant set bit
        pieces &= pieces - 1;                  // remove the least significant set bit

        enumPiece pieceType = board.getPieceType(square);
        if (pieceType != nEmpty) {
            generateMoves(pieceType, square, pseudoMoves, isCaptureOnly);
        }
    }

    // Filter out moves that leave the king in check
    U8 justMovedColour = board.friendlyColour();
    for (short i = 0; i < pseudoMoves.count; i++) {

        Move move = pseudoMoves.moveList[i];

        pushMove(move); // apply the move to the game
        if (!(isInCheck(justMovedColour))) {
            legalMoves.addMove(move); // if the move doesn't leave the king in check, add it to legal moves
        } 
        popMove(); // revert the move after checking

    }

    
    inMoveGeneration = false;

    return legalMoves;
};


MovesStruct Game::generatePseudoLegalMoves(){    

    MovesStruct pseudoMoves;

    inMoveGeneration = true;

    U64 pieces = board.getFriendlyPieces(); // get all friendly pieces

    while (pieces) {
        int square = __builtin_ctzll(pieces);  // get the least significant set bit
        pieces &= pieces - 1;                  // remove the least significant set bit

        enumPiece pieceType = board.getPieceType(square);
        if (pieceType != nEmpty) {
            generateMoves(pieceType, square, pseudoMoves); // generate all pseudo-legal moves
        }
    }

    inMoveGeneration = false;
    return pseudoMoves;
};


void Game::generateMoves(enumPiece pieceType, int square, MovesStruct& pseudoMoves, bool isCaptureOnly){   
    switch (pieceType) {
        case nKings: generateKingMovesForSquare(square, pseudoMoves, isCaptureOnly); break;
        case nKnights: generateKnightMovesForSquare(square, pseudoMoves, isCaptureOnly); break;
        case nBishops: generateBishopMovesForSquare(square, pseudoMoves, isCaptureOnly); break;
        case nRooks: generateRookMovesForSquare(square, pseudoMoves, isCaptureOnly); break;
        case nQueens: generateQueenMovesForSquare(square, pseudoMoves, isCaptureOnly); break;
        case nPawns: generatePawnMovesForSquare(square, pseudoMoves, isCaptureOnly); break;
        default: break;
    }
};

void Game::generateKingMovesForSquare(int square, MovesStruct& pseudoMoves, bool isCaptureOnly) {

    U64 movesBB = tables.kingBB[square];
    U64 attacked = attackedBB(board.enemyColour()); // get attacked squares by enemy pieces
    U64 occupied = board.getAllPieces(); // all occupied squares

    if (isCaptureOnly)  {
        addMovesToStruct(pseudoMoves, square, movesBB & board.getEnemyPieces()); // if capture only, filter to enemy pieces
        return; // if only captures, return early
    }

    // White king on e1 (square 4)
    if (board.friendlyColour() == nWhite && square == 4) {
        // Kingside castle (e1-g1)
        if ((board.gameInfo & WK_CASTLE)) {
            U64 kingSideSquares = (1ULL << 5) | (1ULL << 6); // f1, g1
            U64 kingSideCheck = (1ULL << 4) | (1ULL << 5) | (1ULL << 6); // e1, f1, g1
            
            // Check: squares between king and rook are empty AND not attacked
            if (!(occupied & kingSideSquares) && !(attacked & kingSideCheck)) {
                movesBB |= (1ULL << 6); // Add g1 as valid move
            }
        }
        
        // Queenside castle (e1-c1)
        if ((board.gameInfo & WQ_CASTLE)) {
            U64 queenSideEmpty = (1ULL << 1) | (1ULL << 2) | (1ULL << 3); // b1, c1, d1
            U64 queenSideCheck = (1ULL << 2) | (1ULL << 3) | (1ULL << 4); // c1, d1, e1
            
            // Check: squares between king and rook are empty AND king path not attacked
            if (!(occupied & queenSideEmpty) && !(attacked & queenSideCheck)) {
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
            
            if (!(occupied & kingSideSquares) && !(attacked & kingSideCheck)) {
                movesBB |= (1ULL << 62); // Add g8 as valid move
            }
        }
        
        // Queenside castle (e8-c8)
        if ((board.gameInfo & BQ_CASTLE)) {
            U64 queenSideEmpty = (1ULL << 57) | (1ULL << 58) | (1ULL << 59); // b8, c8, d8
            U64 queenSideCheck = (1ULL << 58) | (1ULL << 59) | (1ULL << 60); // c8, d8, e8
            
            if (!(occupied & queenSideEmpty) && !(attacked & queenSideCheck)) {
                movesBB |= (1ULL << 58); // Add c8 as valid move
            }
        }
    }
    addMovesToStruct(pseudoMoves, square, movesBB);
}

void Game::generateQueenMovesForSquare(int square, MovesStruct& pseudoMoves, bool isCaptureOnly) {
    U64 movesBB = getBishopAttacks(board.getAllPieces(), square) | getRookAttacks(board.getAllPieces(), square);
    isCaptureOnly ? movesBB &= board.getEnemyPieces() : movesBB; // if capture only, filter to enemy pieces
    addMovesToStruct(pseudoMoves, square, movesBB);
}

void Game::generateRookMovesForSquare(int square, MovesStruct& pseudoMoves, bool isCaptureOnly) {
    U64 movesBB = getRookAttacks(board.getAllPieces(), square);
    isCaptureOnly ? movesBB &= board.getEnemyPieces() : movesBB; // if capture only, filter to enemy pieces
    addMovesToStruct(pseudoMoves, square, movesBB);
}

void Game::generateBishopMovesForSquare(int square, MovesStruct& pseudoMoves, bool isCaptureOnly) {
    U64 movesBB = getBishopAttacks(board.getAllPieces(), square);
    isCaptureOnly ? movesBB &= board.getEnemyPieces() : movesBB; // if capture only, filter to enemy pieces
    addMovesToStruct(pseudoMoves, square, movesBB);
}

void Game::generateKnightMovesForSquare(int square, MovesStruct& pseudoMoves, bool isCaptureOnly) {
    U64 movesBB = tables.knightBB[square];
    isCaptureOnly ? movesBB &= board.getEnemyPieces() : movesBB; // if capture only, filter to enemy pieces
    addMovesToStruct(pseudoMoves, square, movesBB);
}

void Game::generatePawnMovesForSquare(int square, MovesStruct& pseudoMoves, bool isCaptureOnly) {
    
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

    if (isCaptureOnly)  {
        addPawnMovesToStruct(pseudoMoves, square, movesBB);
        return; // if only captures, return early
    }

    U64 empty = ~(board.pieceBB[nWhite] | board.pieceBB[nBlack]); // all empty squares
    movesBB |= tables.pawnMovesBB[board.friendlyColour()][square] & empty;

    // Double pawn push
    if (board.friendlyColour() == nWhite && row == 1 && !(movesBB & (1ULL << (square + 8)))) {
        movesBB &= ~(1ULL << (square + 16)); 
    } else if (board.friendlyColour() == nBlack && row == 6 && !(movesBB & (1ULL << (square - 8)))) {
        movesBB &= ~(1ULL << (square - 16)); // remove double pawn push if square is not empty
    }

    addPawnMovesToStruct(pseudoMoves, square, movesBB);
}

U64 Game::attackedBB(U8 enemyColour) {
    // Check if the square is attacked by any enemy piece
    U64 pieces = board.pieceBB[enemyColour];
    U64 attacked = 0ULL;

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
                attacked |= getBishopAttacks(board.getAllPieces(), square);
                break;
            case nRooks:
                attacked |= getRookAttacks(board.getAllPieces(), square);
                break;
            case nQueens:
                attacked |= getBishopAttacks(board.getAllPieces(), square);
                attacked |= getRookAttacks(board.getAllPieces(), square);
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


void Game::addMovesToStruct(MovesStruct& moves, int square, U64 movesBB) {
    // remove moves that land on friendly pieces
    movesBB &= ~board.pieceBB[board.friendlyColour()];
    
    while(movesBB) {
        int to = __builtin_ctzll(movesBB);
        movesBB &= movesBB - 1;

        Move move(square, to, board.getEnPassantSquare(), board.getPieceType(square), board.getPieceType(to));
        moves.addMove(move);
    }
}

void Game::addPawnMovesToStruct(MovesStruct& moves, int square, U64 movesBB) {
    enumPiece friendlyColour = board.friendlyColour();
    // remove moves that land on friendly pieces
    movesBB &= ~board.pieceBB[friendlyColour];
    
    while(movesBB) {
        int to = __builtin_ctzll(movesBB);
        movesBB &= movesBB - 1;

        // promotion check
        if (to >= 56 || to <= 7){
            Move move1(square, to, board.getEnPassantSquare(), board.getPieceType(square), board.getPieceType(to), nKnights);
            Move move2(square, to, board.getEnPassantSquare(), board.getPieceType(square), board.getPieceType(to), nBishops);
            Move move3(square, to, board.getEnPassantSquare(), board.getPieceType(square), board.getPieceType(to), nRooks);
            Move move4(square, to, board.getEnPassantSquare(), board.getPieceType(square), board.getPieceType(to), nQueens);
            moves.addMove(move1);
            moves.addMove(move2);
            moves.addMove(move3);   
            moves.addMove(move4);
        } else {
            Move move(square, to, board.getEnPassantSquare(), board.getPieceType(square), board.getPieceType(to));
            moves.addMove(move);
        }
    }
}

bool Game::isInCheck() {
    return isInCheck(board.friendlyColour());
}

bool Game::isInCheck(U8 colour) {
    // Find the king's bitboard for the given colour
    U64 kingBB = 0ULL;
    U8 enemyColour;

    if (colour == nWhite) {
        kingBB = board.getWhiteKing();
        enemyColour = nBlack;
    } else {
        kingBB = board.getBlackKing();
        enemyColour = nWhite;
    }

    // If there is no king on the board, not in check (defensive)
    if (kingBB == 0) return false;

    // Use MoveGenerator to get all squares attacked by the enemy
    U64 attacked = attackedBB(enemyColour);

    // If the king's square is attacked, return true
    return (kingBB & attacked) != 0;
}

bool Game::isLegal(const U8 from, const U8 to) {
    // Basic validation
    enumPiece pieceType = board.getPieceType(from);
    if (pieceType == nEmpty) return false;
    if (board.getColourType(from) != board.friendlyColour()) return false;

    // Check if this is a valid move according to chess rules
    MovesStruct pseudoMoves;
    generateMoves(pieceType, from, pseudoMoves);

    // Check if 'to' square is in the generated pseudo-legal moves
    bool foundMove = false;
    for (int i = 0; i < pseudoMoves.count; i++) {
        Move move = pseudoMoves.moveList[i];
        if (move.getTo() == to) {
            foundMove = true;
            break;
        }
    }

    if (!foundMove) return false;
    enumPiece ogMovingColour = board.friendlyColour();
    // If move is in pseudo-legal moves, check if it leaves king in check
    Move move(from, to, board.getEnPassantSquare(), pieceType, board.getPieceType(to));

    pushMove(move); // apply the move to the board
    bool inCheck = isInCheck(ogMovingColour);
    popMove(); // revert the move
    return !inCheck;
}

bool Game::isFiftyMoveRule() const {
    // Check if the halfmove clock is 100 or more
    return ((board.gameInfo & MOVE_MASK) >> 6) >= 100;
}


// ...existing code...
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


// bool Game::isThreefoldRepetition() const {
//     U64 currentHash = board.getHash();
//     int count = 1;

//     if (useStackHistory) {
//         for (int i = searchDepth - 1; i >= 0; i--){

//             if (searchHistory[i].hash == currentHash) {
//                 if (++count >= 3) return true;

//                 // if (!(searchHistory[i].gameInfo & MOVE_MASK)) return false; // if halfmove clock is not set, it's not a repetition
//                 // if (searchHistory[i].move.isCapture()) return false;
//                 // if ( board.getPieceType(searchHistory[i].move.getFrom()) == nPawns ) return false;
//             }
//         }
//     }

//     // otherwise use linked list implementation
//     for (HistoryNode* node = historyTail; node != nullptr; node = node->prev) {
//         if (node->state.hash == currentHash) {
//             if (++count >= 3) return true;

//             // if (!(node->state.gameInfo & MOVE_MASK)) return false; // if halfmove clock is not set, it's not a repetition
//             // if (node->state.move.isCapture()) return false;
//             // if ( board.getPieceType(node->state.move.getFrom()) == nPawns ) return false;
//         }
//     }
//     return false;   
// }


void Game::displayBitboard(U64 bitboard, int square, char symbol) const {   
    std::cout << "\n";    
    for (int r = 7; r >= 0; r --){
        for (int c = 0; c < 8; c ++){
            int square_i = r * 8 + c;
            char piece = '.';

            if (bitboard >> square_i & 1){
                piece = 'x';
            } 
            if (square_i == square){
                piece = symbol;
            }
            std::cout<< piece << " ";
        }
        std::cout<< "\n";
    }
}




