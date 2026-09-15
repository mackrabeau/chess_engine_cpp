#include "../board.h"
#include "../game.h"
#include "../move.h"
#include "test_config.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>

namespace {

int g_failedTests = 0;
int g_passedTests = 0;

struct BoardSnapshot {
	U64 pieceBB[8];
	U16 gameInfo;
	U64 hash;
};

BoardSnapshot snapshotBoard(const Board& board) {
	BoardSnapshot snapshot{};
	std::memcpy(snapshot.pieceBB, board.pieceBB, sizeof(snapshot.pieceBB));
	snapshot.gameInfo = board.gameInfo;
	snapshot.hash = board.getHash();
	return snapshot;
}

bool sameBoardState(const Board& board, const BoardSnapshot& snapshot) {
	if (board.gameInfo != snapshot.gameInfo) return false;
	if (board.getHash() != snapshot.hash) return false;
	return std::memcmp(board.pieceBB, snapshot.pieceBB, sizeof(snapshot.pieceBB)) == 0;
}

U64 recomputeHash(const Board& board) {
	Board copy = board;
	copy.calculateHash();
	return copy.getHash();
}

void expect(bool condition, const std::string& message) {
	if (!condition) {
		throw std::runtime_error(message);
	}
}

long long perft(Game& game, int depth) {
	if (depth == 0) return 1;

	MovesStruct moves = game.generateAllLegalMoves();
	if (depth == 1) {
		return moves.getNumMoves();
	}

	long long nodes = 0;
	for (int i = 0; i < moves.getNumMoves(); ++i) {
		Move move = moves.getMove(i);
		game.pushMove(move);
		nodes += perft(game, depth - 1);
		game.popMove();
	}
	return nodes;
}

void test_checkmate_stalemate() {
	Game checkmateGame("rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 0 3");
	expect(checkmateGame.generateAllLegalMoves().getNumMoves() == 0, "Checkmate should have 0 legal moves");

	Game stalemateGame("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");
	expect(stalemateGame.generateAllLegalMoves().getNumMoves() == 0, "Stalemate should have 0 legal moves");
}

void test_special_moves_exist() {
	{
		Game game("8/8/8/3pP3/8/8/8/8 w - d6 0 1");
		MovesStruct legalMoves = game.generateAllLegalMoves();
		bool foundEnPassant = false;
		for (int i = 0; i < legalMoves.getNumMoves(); ++i) {
			if (isEPCapture(legalMoves.getMove(i))) {
				foundEnPassant = true;
				break;
			}
		}
		expect(foundEnPassant, "Expected a legal en-passant capture");
	}

	{
		Game game("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
		MovesStruct legalMoves = game.generateAllLegalMoves();
		bool foundKingCastle = false;
		bool foundQueenCastle = false;

		for (int i = 0; i < legalMoves.getNumMoves(); ++i) {
			Move move = legalMoves.getMove(i);
			if (isKingCastle(move)) foundKingCastle = true;
			if (isQueenCastle(move)) foundQueenCastle = true;
		}

		expect(foundKingCastle, "Expected legal king-side castling");
		expect(foundQueenCastle, "Expected legal queen-side castling");
	}

	{
		Game game("8/P7/8/8/8/8/8/8 w - - 0 1");
		MovesStruct legalMoves = game.generateAllLegalMoves();
		bool foundPromotion = false;

		for (int i = 0; i < legalMoves.getNumMoves(); ++i) {
			Move move = legalMoves.getMove(i);
			if (isPromotion(move) || isPromoCapture(move)) {
				foundPromotion = true;
				break;
			}
		}

		expect(foundPromotion, "Expected promotion move");
	}
}

void test_insufficient_material() {
	Game kVsK("8/8/8/8/8/8/8/K6k w - - 0 1");
	expect(kVsK.isInsufficientMaterial(), "K vs K should be insufficient material");

	Game kbVsK("8/8/8/8/8/8/8/KB5k w - - 0 1");
	expect(kbVsK.isInsufficientMaterial(), "K+B vs K should be insufficient material");

	Game knVsK("8/8/8/8/8/8/8/KN5k w - - 0 1");
	expect(knVsK.isInsufficientMaterial(), "K+N vs K should be insufficient material");

	Game kbVsKbSameColor("8/8/8/8/8/8/8/KB5k w - - 0 1");
	kbVsKbSameColor.setPosition("8/8/8/8/8/8/7b/KB5k w - - 0 1");
	expect(kbVsKbSameColor.isInsufficientMaterial(), "K+B vs K+B (same color bishops) should be insufficient material");

	Game krVsK("8/8/8/8/8/8/8/KR5k w - - 0 1");
	expect(!krVsK.isInsufficientMaterial(), "K+R vs K should not be insufficient material");
}

void verifyHashConsistencyOnAllMoves(const std::string& fen) {
	Game game(fen);
	MovesStruct moves = game.generateAllLegalMoves();

	for (int i = 0; i < moves.getNumMoves(); ++i) {
		const BoardSnapshot before = snapshotBoard(game.board);
		const Move move = moves.getMove(i);

		game.pushMove(move);
		expect(game.board.getHash() == recomputeHash(game.board), "Incremental hash mismatch after pushMove()");

		game.popMove();
		expect(sameBoardState(game.board, before), "Board state not restored after pushMove()/popMove()");
		expect(game.board.getHash() == recomputeHash(game.board), "Incremental hash mismatch after popMove()");
	}
}

void verifyDeterministicRandomWalk(const std::string& fen, int plies, std::mt19937& rng) {
	Game game(fen);
	game.enableFastMode();
	int appliedMoves = 0;

	for (int ply = 0; ply < plies; ++ply) {
		MovesStruct legal = game.generateAllLegalMoves();
		if (legal.getNumMoves() == 0) break;

		std::uniform_int_distribution<int> dist(0, legal.getNumMoves() - 1);
		Move move = legal.getMove(dist(rng));

		const BoardSnapshot before = snapshotBoard(game.board);
		game.pushMove(move);
		expect(game.board.getHash() == recomputeHash(game.board), "Incremental hash mismatch during random walk pushMove()");

		game.popMove();
		expect(sameBoardState(game.board, before), "Board state not restored during random walk popMove()");
		expect(game.board.getHash() == recomputeHash(game.board), "Incremental hash mismatch during random walk popMove()");

		// Re-apply once to keep advancing the walk deterministically.
		game.pushMove(move);
		++appliedMoves;
		expect(game.board.getHash() == recomputeHash(game.board), "Incremental hash mismatch while advancing random walk");
	}

	while (appliedMoves-- > 0) {
		game.popMove();
	}

	game.disableFastMode();
}

void test_make_unmake_and_hash_invariants() {
	for (const auto& fen : test_config::kInvariantFens) {
		verifyHashConsistencyOnAllMoves(fen);
	}

	std::mt19937 rng(test_config::RANDOM_SEED);
	for (const auto& fen : test_config::kInvariantFens) {
		verifyDeterministicRandomWalk(fen, 32, rng);
	}
}

void test_perft_reference_nodes() {
	for (const auto& test : test_config::kPerftCases) {
		Game game(test.fen);
		long long nodes = perft(game, test.depth);
		expect(nodes == test.expected,
			   "Perft mismatch for " + test.name + ": expected " + std::to_string(test.expected) +
			   ", got " + std::to_string(nodes));
	}
}

void runTest(const std::string& name, void (*fn)()) {
	try {
		fn();
		++g_passedTests;
		std::cout << "[PASS] " << name << std::endl;
	} catch (const std::exception& ex) {
		++g_failedTests;
		std::cerr << "[FAIL] " << name << " - " << ex.what() << std::endl;
	} catch (...) {
		++g_failedTests;
		std::cerr << "[FAIL] " << name << " - unknown exception" << std::endl;
	}
}

} // namespace

int main() {
	runTest("checkmate_stalemate", test_checkmate_stalemate);
	runTest("special_moves", test_special_moves_exist);
	runTest("insufficient_material", test_insufficient_material);
	runTest("make_unmake_hash_invariants", test_make_unmake_and_hash_invariants);
	runTest("perft_reference_nodes", test_perft_reference_nodes);

	std::cout << "Passed: " << g_passedTests << ", Failed: " << g_failedTests << std::endl;
	return g_failedTests == 0 ? 0 : 1;
}
