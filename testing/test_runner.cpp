#include "../board.h"
#include "../game.h"
#include "../move.h"
#include "../search.h"
#include "../transposition.h"
#include "engine_interface.h"
#include "game_runner.h"
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
	U16 fullmoveNumber;
	U64 hash;
};

BoardSnapshot snapshotBoard(const Board& board) {
	BoardSnapshot snapshot{};
	std::memcpy(snapshot.pieceBB, board.pieceBB, sizeof(snapshot.pieceBB));
	snapshot.gameInfo = board.gameInfo;
	snapshot.fullmoveNumber = board.fullmoveNumber;
	snapshot.hash = board.getHash();
	return snapshot;
}

bool sameBoardState(const Board& board, const BoardSnapshot& snapshot) {
	if (board.gameInfo != snapshot.gameInfo) return false;
	if (board.fullmoveNumber != snapshot.fullmoveNumber) return false;
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
		Game game("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
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
		Game game("4k3/P7/8/8/8/8/8/4K3 w - - 0 1");
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

void test_missing_king_is_safe() {
	Game game("8/8/8/3pP3/8/8/8/8 w - d6 0 1");
	MovesStruct legalMoves = game.generateAllLegalMoves();
	expect(legalMoves.getNumMoves() == 0, "Board with no king should not crash and should produce no legal moves");
	const bool isChecked = game.isInCheck();
	expect(!isChecked, "Board with no king should not report check");
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

Move findMove(Game& game, const std::string& uciMove) {
	MovesStruct legalMoves = game.generateAllLegalMoves();
	for (int i = 0; i < legalMoves.getNumMoves(); ++i) {
		if (moveToString(legalMoves.getMove(i)) == uciMove) {
			return legalMoves.getMove(i);
		}
	}
	return MOVE_NONE;
}

void playMove(Game& game, const std::string& uciMove) {
	Move move = findMove(game, uciMove);
	expect(move != MOVE_NONE, "Expected legal move " + uciMove);
	game.pushMove(move);
}

void test_fen_and_counter_contract() {
	const std::string fen = "r3k2r/8/8/8/8/8/R3K2R/8 b KQkq e3 99 42";
	Board board(fen);
	expect(board.toString() == fen, "FEN should round-trip all position counters and rights; got " + board.toString());
	expect(board.getHalfMoveClock() == 99, "FEN halfmove clock should be preserved");
	expect(board.fullmoveNumber == 42, "FEN fullmove number should be preserved");

	Game game(fen);
	Move move = findMove(game, "a8a7");
	expect(move != MOVE_NONE, "Expected a legal black quiet move");
	game.pushMove(move);
	expect(game.board.getHalfMoveClock() == 100, "Black quiet move should increment the halfmove clock");
	expect(game.board.fullmoveNumber == 43, "Black move should increment the fullmove number");
	expect(game.isFiftyMoveRule(), "Halfmove clock at 100 should trigger the fifty-move rule");
	game.popMove();
	expect(game.board.toString() == fen, "Pop should restore the complete FEN state");
}

void test_repetition_history() {
	Game game("7k/8/8/8/8/8/R7/6K1 w - - 0 1");
	const std::vector<std::string> cycle = {"a2a3", "h8g8", "a3a2", "g8h8"};
	for (int repetition = 0; repetition < 2; ++repetition) {
		for (const auto& move : cycle) playMove(game, move);
	}
	expect(game.isThreefoldRepetition(), "Repeated position should be detected after three occurrences");

	for (int i = 0; i < 4; ++i) game.popMove();
	expect(!game.isThreefoldRepetition(), "Undoing one cycle should remove the third occurrence");
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

void test_board_state_contract() {
	Game legal("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
	expect(legal.validateBoardState(), "Starting position should satisfy the board-state contract");

	Game noKing("8/8/8/8/8/8/8/8 w - - 0 1");
	expect(!noKing.validateBoardState(), "Board with no kings should violate the board-state contract");

	Game game("4k3/8/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
	const auto before = snapshotBoard(game.board);
	MovesStruct legalMoves = game.generateAllLegalMoves();
	expect(legalMoves.getNumMoves() > 0, "Expected legal moves in the en passant position");
	for (int i = 0; i < legalMoves.getNumMoves(); ++i) {
		Move move = legalMoves.getMove(i);
		game.pushMove(move);
		expect(game.validateBoardState(), "Board state contract should hold after a legal pushMove");
		game.popMove();
		expect(sameBoardState(game.board, before), "Board state should restore exactly after a legal move");
		expect(game.validateBoardState(), "Board state contract should hold after a legal popMove");
		break;
	}
}

void test_search_reproducibility_baseline() {
	Game game("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
	resetSearchStats();
	g_transpositionTable.clear();
	Move first = searchAtDepth(game, 3, nullptr);

	resetSearchStats();
	g_transpositionTable.clear();
	Move second = searchAtDepth(game, 3, nullptr);

	expect(first != MOVE_NONE, "Search should find a legal move from the start position");
	expect(first == second, "Search should be reproducible for the same position and depth");
	expect(game.validateBoardState(), "Search baseline should not corrupt the board-state contract");
}

void test_deterministic_benchmark_harness() {
	for (const auto& benchmark : test_config::kBenchmarkCases) {
		Game game(benchmark.fen);
		std::vector<std::pair<Move, long>> results;
		results.reserve(2);

		for (int run = 0; run < 2; ++run) {
			resetSearchStats();
			g_transpositionTable.clear();
			const Move best = searchAtDepth(game, benchmark.depth, nullptr);
			results.push_back({best, g_nodeCount});
		}

		expect(results[0].first != MOVE_NONE, "Benchmark search should find a legal move for " + benchmark.name);
		expect(results[0].first == results[1].first, "Benchmark best move should be deterministic for " + benchmark.name);
		expect(results[0].second == results[1].second, "Benchmark node counts should be deterministic for " + benchmark.name);
		expect(game.validateBoardState(), "Benchmark search should not corrupt the board-state contract for " + benchmark.name);
	}
}

void test_uci_engine_boundary() {
	EngineInterface engine("./engine", "test-engine");
	expect(engine.initialize(), "UCI engine should initialize");
	expect(engine.isReady(), "UCI engine should report ready");
	expect(engine.setPosition("startpos"), "UCI engine should accept start position");

	std::string bestMove;
	SearchStats stats;
	expect(engine.getBestMove(10, bestMove, stats), "UCI engine should return a best move");
	expect(bestMove != "0000", "UCI engine should return a move from the start position");

	Game game;
	MovesStruct legalMoves = game.generateAllLegalMoves();
	bool foundMove = false;
	for (int i = 0; i < legalMoves.getNumMoves(); ++i) {
		if (moveToString(legalMoves.getMove(i)) == bestMove) {
			foundMove = true;
			break;
		}
	}
	expect(foundMove, "UCI engine best move should be legal in the local game state");
	engine.quit();

	EngineInterface terminalWhite("./engine", "terminal-white");
	EngineInterface terminalBlack("./engine", "terminal-black");
	expect(terminalWhite.initialize(), "White terminal-test engine should initialize");
	expect(terminalBlack.initialize(), "Black terminal-test engine should initialize");
	GameRunner runner(&terminalWhite, &terminalBlack,
		"7k/5Q2/6K1/8/8/8/8/8 b - - 0 1",
		"terminal-white", "terminal-black");
	GameResult result = runner.runGame(1);
	expect(result.result == "1/2-1/2", "Terminal stalemate should be reported as a draw");
	expect(result.reason == "Game ended", "Terminal game should report its termination reason");
	terminalWhite.quit();
	terminalBlack.quit();
}

void test_perft_reference_nodes() {
	for (const auto& test : test_config::kPerftCases) {
		Game game(test.fen);
		long long nodes = perft(game, test.depth);
		std::string detail = "Perft mismatch for " + test.name + ": expected " + std::to_string(test.expected) +
			", got " + std::to_string(nodes);
		if (nodes != test.expected && test.depth == 1) {
			MovesStruct moves = game.generateAllLegalMoves();
			detail += " moves=";
			for (int i = 0; i < moves.getNumMoves(); ++i) {
				if (i > 0) detail += ",";
				detail += moveToString(moves.getMove(i));
			}
		}
		expect(nodes == test.expected, detail);
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
	runTest("missing_king_is_safe", test_missing_king_is_safe);
	runTest("board_state_contract", test_board_state_contract);
	runTest("search_reproducibility_baseline", test_search_reproducibility_baseline);
	runTest("deterministic_benchmark_harness", test_deterministic_benchmark_harness);
	runTest("uci_engine_boundary", test_uci_engine_boundary);
	runTest("insufficient_material", test_insufficient_material);
	runTest("fen_and_counter_contract", test_fen_and_counter_contract);
	runTest("repetition_history", test_repetition_history);
	runTest("make_unmake_hash_invariants", test_make_unmake_and_hash_invariants);
	runTest("perft_reference_nodes", test_perft_reference_nodes);

	std::cout << "Passed: " << g_passedTests << ", Failed: " << g_failedTests << std::endl;
	return g_failedTests == 0 ? 0 : 1;
}
