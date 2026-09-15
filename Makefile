CC := g++
CXXFLAGS := -std=c++17 -O3 -Wall -Wextra -pedantic
CPPFLAGS := -I.

SRC := bitboard.cpp board.cpp game.cpp move.cpp movetables.cpp evaluation.cpp search.cpp transposition.cpp
HEADERS := bitboard.h board.h game.h move.h movetables.h evaluation.h search.h transposition.h types.h

ENGINE_SRC := engine.cpp $(SRC)
PERFT_SRC := perft.cpp $(SRC)
TEST_SRC := testing/test_runner.cpp $(SRC)

ENGINE_OBJ := $(ENGINE_SRC:.cpp=.o)
PERFT_OBJ := $(PERFT_SRC:.cpp=.o)
TEST_OBJ := $(TEST_SRC:.cpp=.o)

.PHONY: all clean engine perft tests

all: engine perft tests

engine: $(ENGINE_OBJ)
	$(CC) $(CXXFLAGS) -o engine $(ENGINE_OBJ)

perft: $(PERFT_OBJ)
	$(CC) $(CXXFLAGS) -o perft $(PERFT_OBJ)

tests: $(TEST_OBJ)
	$(CC) $(CXXFLAGS) -o tests $(TEST_OBJ)

%.o: %.cpp $(HEADERS)
	$(CC) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(ENGINE_OBJ) $(PERFT_OBJ) $(TEST_OBJ) engine perft tests


