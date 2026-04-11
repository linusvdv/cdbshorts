#pragma once
#include "external/chess.hpp"

using namespace chess;

enum class ProgressOutcome {
    kLater,
    kSame,
    kEarlier,
};

constexpr Bitboard flip_vertical(Bitboard b) {
  b = ((b << 56)) | ((b << 40) & 0x00FF000000000000ULL) |
      ((b << 24) & 0x0000FF0000000000ULL) | ((b << 8) & 0x000000FF00000000ULL) |
      ((b >> 8) & 0x00000000FF000000ULL) | ((b >> 24) & 0x0000000000FF0000ULL) |
      ((b >> 40) & 0x000000000000FF00ULL) | ((b >> 56));
  return b;
}

inline ProgressOutcome game_progress(const Board& board1, const Board& board2) {
    for (const auto& color : {Color::WHITE, Color::BLACK}) {
        // count the number of pieces
        // order is important! pawn has to be checked first!
        for (const auto& piece_type : {PieceType::PAWN, PieceType::KNIGHT, PieceType::BISHOP, PieceType::ROOK, PieceType::QUEEN}) {
            auto pc1 = board1.pieces(piece_type, color).count();
            auto pc2 = board2.pieces(piece_type, color).count();
            if (pc1 < pc2) {
                return ProgressOutcome::kLater;
            }
            if (pc1 > pc2) {
                return ProgressOutcome::kEarlier;
            }
        }

        // count light squared bishops (dark are implicitly checked)
        constexpr Bitboard kLightSquares = 0x55AA55AA55AA55AA;
        auto pc1 = (board1.pieces(PieceType::BISHOP, color)&kLightSquares).count();
        auto pc2 = (board2.pieces(PieceType::BISHOP, color)&kLightSquares).count();
        if (pc1 < pc2) {
            return ProgressOutcome::kLater;
        }
        if (pc1 > pc2) {
            return ProgressOutcome::kEarlier;
        }

        // Consider pawn structure (note the number of pawns is equal when we get here)
        auto pawns1 = board1.pieces(PieceType::PAWN, color);
        auto pawns2 = board2.pieces(PieceType::PAWN, color);
        if (color == Color::BLACK) {
            pawns1 = flip_vertical(pawns1);
            pawns2 = flip_vertical(pawns2);
        }
        while (pawns1) {
            Square sq1 = Square(pawns1.pop());
            Square sq2 = Square(pawns2.pop());
            if (sq1 < sq2) {
                return ProgressOutcome::kEarlier;
            }
            if (sq1 > sq2) {
                return ProgressOutcome::kLater;
            }
        }

        // Consider en passant square
        Square ep1 = board1.enpassantSq();
        Square ep2 = board2.enpassantSq();
        if (ep1 != Square::underlying::NO_SQ && ep2 == Square::underlying::NO_SQ) {
            return ProgressOutcome::kEarlier;
        }
        if (ep1 == Square::underlying::NO_SQ && ep2 != Square::underlying::NO_SQ) {
            return ProgressOutcome::kLater;
        }
        if (ep1 != Square::underlying::NO_SQ && ep2 != Square::underlying::NO_SQ) {
            // both have a en passent square the choice of earlier and later is not important
            if (ep1 < ep2) {
                return ProgressOutcome::kEarlier;
            }
            if (ep1 > ep2) {
                return ProgressOutcome::kLater;
            }
        }

        // Check castling rights
        Board::CastlingRights cr1 = board1.castlingRights();
        Board::CastlingRights cr2 = board2.castlingRights();
        for (const auto &side : {Board::CastlingRights::Side::KING_SIDE,
            Board::CastlingRights::Side::QUEEN_SIDE}) {
            if (cr1.has(color, side) && !cr2.has(color, side)) {
                return ProgressOutcome::kEarlier;
            }
            if (!cr1.has(color, side) && cr2.has(color, side)) {
                return ProgressOutcome::kLater;
            }
        }
    }
    return ProgressOutcome::kSame;
}


// returns true if the first position can only appear after the second position in the game
struct ProgressLater {
    bool operator()(const PackedBoard& lhs, const PackedBoard& rhs) const {
        return game_progress(Board::Compact::decode(lhs), Board::Compact::decode(rhs)) == ProgressOutcome::kLater;
    }
};


// returns true if the first position can only appear before the second position in the game
struct ProgressEarlier {
    bool operator()(const PackedBoard& lhs, const PackedBoard& rhs) const {
        return game_progress(Board::Compact::decode(lhs), Board::Compact::decode(rhs)) == ProgressOutcome::kEarlier;
    }
};


// returns true if the first position can only appear after the second position in the game
// same progress is sorted in reverse lexicographical order
struct LaterBoard {
    bool operator()(const PackedBoard& lhs, const PackedBoard& rhs) const {
        ProgressOutcome progression_outcome = game_progress(Board::Compact::decode(lhs), Board::Compact::decode(rhs));
        if (progression_outcome == ProgressOutcome::kLater) {
            return true;
        }
        if (progression_outcome == ProgressOutcome::kEarlier) {
            return false;
        }
        return lhs > rhs;
    }
};


// returns true if the first position can only appear before the second position in the game
// same progress is sorted lexicographically
struct EarlierBoard {
    bool operator()(const PackedBoard& lhs, const PackedBoard& rhs) const {
        ProgressOutcome progression_outcome = game_progress(Board::Compact::decode(lhs), Board::Compact::decode(rhs));
        if (progression_outcome == ProgressOutcome::kLater) {
            return false;
        }
        if (progression_outcome == ProgressOutcome::kEarlier) {
            return true;
        }
        return lhs < rhs;
    }
};
