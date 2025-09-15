# Chess Engine

## Features

- **Complete move generation** with legal move validation
- **Alpha-beta search algorithm** with configurable depth
- **Full chess rules support**:
  - Castling (kingside and queenside)
  - En passant captures
  - Pawn promotion (all piece types)
  - Check, checkmate, and stalemate detection
  - Threefold repetition detection
  - 50-move rule implementation
- **FEN string parsing** and generation
- **Performance testing** with perft functionality
- **Binary move representation** with detailed visualization

```
  ├── engine.cpp          # Main engine interface
  ├── board.cpp           # Board representation
  ├── board.h
  ├── movegenerator.cpp   # Move generation logic
  ├── movegenerator.h
  ├── search.cpp          # Alpha-beta search
  ├── search.h
  ├── evaluation.cpp      # Position evaluation
  ├── evaluation.h
  ├── game.cpp           # Game state management
  ├── game.h
  ├── movetables.cpp     # Pre-computed move tables
  ├── movetables.h
  ├── types.h            # Type definitions
  ├── move.h             # Move representation
  ├── perft.cpp          # Performance testing
```

## 🛠️ Building and Installation

### Prerequisites
- **C++11** compiler (g++ or clang)

### Build the C++ Engine

Compile the main engine:
```bash
g++ -std=c++11 game.cpp board.cpp movegenerator.cpp movetables.cpp search.cpp evaluation.cpp engine.cpp -o engine
```

Compile the performance tester (optional):
```bash
g++ -std=c++11 game.cpp board.cpp movegenerator.cpp movetables.cpp search.cpp evaluation.cpp perft.cpp -o perft
```

Example:
```bash
./engine
engine reset
engine move e2e4
engine best
engine print
engine state
```

```bash
.perft 4 "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
```

### Expected Perft Results (Starting Position)
| Depth | Nodes       |
|-------|-------------|
| 1     | 20          |
| 2     | 400         |
| 3     | 8,902       |
| 4     | 197,281     |
| 5     | 4,865,609   |

## 🏗️ Technical Details

### Move Representation
Moves are encoded as 16-bit integers:
- **Bits 0-5**: Target square (0-63)
- **Bits 6-11**: Source square (0-63)  
- **Bits 12-15**: Move flags (piece type, special moves)

### Board Representation
- **Bitboards**: 64-bit integers representing piece positions
- **8x8 array mapping**: Square indices 0-63 (a1=0, h8=63)
- **Compact game state**: Castling rights, en passant, turn stored in single 16-bit integer

### Search Algorithm
- **Alpha-beta pruning** with configurable depth
- **Move ordering** for better pruning efficiency (MVV-LVA, killer moves)
- **Quiescence search** for tactical positions
- **Iterative deepening** for time management
- **Transposition tables** for storing and reusing previous search results
- **Time-based search cutoff** for responsive play

## Future Improvements

- [ ] **Opening book** integration
- [ ] **Endgame tablebase** support
- [ ] **UCI protocol** compatibility
- [ ] **Position evaluation** improvements
- [ ] **Parallel search** with multiple threads

## Usage Examples

### Running the Engine

First, compile and start the engine:
```bash
cd chess/board
g++ -std=c++11 -o engine [source files]
./engine
```

The engine accepts commands via standard input. Each command starts with a request ID (any number) followed by the command and its parameters.

### Basic Commands
```bash
# Set a position using FEN notation
1 engine set_position rnbqkbnr/pppppppp/8/8/8/5P2/PPPPP1PP/RNBQKBNR b KQkq - 0 1

# Get the best move for current position
2 engine best

# Make a move (from e2 to e4)
3 engine move e2e4

# Check current game state (ongoing, checkmate, draws, etc.)
4 engine state

# Get current position evaluation
5 engnine eval

# Get current position as FEN string
6 engine position

# Reset to starting position
7 engine reset

# Display board in terminal
8 engine print

# Quit the engine
9 engine quit
```

### Debug Commands
```bash
# Get transposition table statistics
10 engine tt_stats

# Debug best move evaluation
11 engine debug_best_move
```

**Built by [mackrabeau](https://github.com/mackrabeau)**
