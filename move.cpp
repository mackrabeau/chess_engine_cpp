#include "move.h"

std::string moveToString(Move m) {
    std::string moveStr;
    moveStr += 'a' + (getFrom(m) % 8); // file
    moveStr += '1' + (getFrom(m) / 8); // rank
    moveStr += 'a' + (getTo(m) % 8); // file
    moveStr += '1' + (getTo(m) / 8); // rank

    if (isPromotion(m) || isPromoCapture(m)) {
        char promoChar;
        switch (getPromotionPiece(m)) {
            case nQueens:  promoChar = 'q'; break;
            case nKnights: promoChar = 'n'; break;
            case nBishops: promoChar = 'b'; break;
            case nRooks:   promoChar = 'r'; break;
            default: promoChar = 'E'; // default to "E" if something goes wrong
        }
        moveStr += promoChar; // default to queen promotion
    }

    return moveStr;
}