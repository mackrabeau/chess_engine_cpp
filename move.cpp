#include "move.h"


std::string Move::toString() const {
    std::string moveStr;
    moveStr += 'a' + (getFrom() % 8); // file
    moveStr += '1' + (getFrom() / 8); // rank
    moveStr += 'a' + (getTo() % 8); // file
    moveStr += '1' + (getTo() / 8); // rank

    if (isPromotion() || isPromoCapture()) {
        char promoChar;
        switch (getPromotionPiece()) {
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