#ifndef TEST_CONFIG_H
#define TEST_CONFIG_H

#include <string>
#include <vector>

namespace test_config {

struct PerftCase {
	std::string name;
	std::string fen;
	int depth;
	long long expected;
};

static const unsigned int RANDOM_SEED = 20260915u;

static const std::vector<PerftCase> kPerftCases = {
	{"startpos_d4", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 4, 197281},
	{"kiwipete_d3", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 3, 97862},
	{"startpos_d5", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 5, 4865609}
};

struct BenchmarkCase {
	std::string name;
	std::string fen;
	int depth;
};

static const std::vector<BenchmarkCase> kBenchmarkCases = {
	{"startpos_depth_3", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 3},
	{"kiwipete_depth_2", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 2},
	{"en_passant_depth_2", "4k3/8/8/8/3pP3/8/8/8/4K3 w - d6 0 1", 2}
};

static const std::vector<std::string> kInvariantFens = {
	"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
	"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
	"4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1",
	"4k3/8/8/8/8/8/P7/4K3 w - - 0 1"
};

} // namespace test_config

#endif // TEST_CONFIG_H
