CC := g++
CXXFLAGS := -std=c++17 -O3 -Wall -Wextra -pedantic

SRC := bitboard.cpp board.cpp game.cpp move.cpp movetables.cpp evaluation.cpp search.cpp transposition.cpp
HEADERS := bitboard.h board.h game.h move.h movetables.h evaluation.h search.h transposition.h types.h

TESTING_SRC := testing/engine_interface.cpp testing/game_runner.cpp testing/game_recorder.cpp testing/san_converter.cpp
TESTING_HEADERS := testing/engine_interface.h testing/game_runner.h testing/game_recorder.h testing/san_converter.h testing/test_config.h

ENGINE_SRC := engine.cpp $(SRC)
PERFT_SRC := perft.cpp $(SRC)
TEST_RUNNER_SRC := testing/test_runner.cpp $(TESTING_SRC) $(SRC)

ENGINE_OBJ := $(ENGINE_SRC:.cpp=.o)
PERFT_OBJ := $(PERFT_SRC:.cpp=.o)
TEST_RUNNER_OBJ := $(TEST_RUNNER_SRC:.cpp=.o)

.PHONY: all clean engine perft test_runner

all: engine perft test_runner

engine: $(ENGINE_OBJ)
	$(CC) $(CXXFLAGS) -o engine $(ENGINE_OBJ)

perft: $(PERFT_OBJ)
	$(CC) $(CXXFLAGS) -o perft $(PERFT_OBJ)

test_runner: $(TEST_RUNNER_OBJ)
	$(CC) $(CXXFLAGS) -o test_runner $(TEST_RUNNER_OBJ)

%.o: %.cpp $(HEADERS)
	$(CC) $(CXXFLAGS) -c $< -o $@

testing/%.o: testing/%.cpp $(HEADERS) $(TESTING_HEADERS)
	$(CC) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(ENGINE_OBJ) $(PERFT_OBJ) $(TEST_RUNNER_OBJ) engine perft test_runner


