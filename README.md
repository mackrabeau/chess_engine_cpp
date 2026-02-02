# Chess Engine

Modern C++ bitboard chess engine with a UCI interface, iterative-deepening search, NNUE evaluation, and reinforcement learning training infrastructure.

## Features

- **Complete rule set**: legal move generation, promotions, castling, en passant, draw rules (50-move, repetition).
- **Bitboard move generation** backed by precomputed tables and magic-bitboard style sliding attacks.
- **Search**: iterative-deepening alpha-beta with killer moves, MVV-LVA ordering, quiescence, and a transposition table (configurable hash size).
- **NNUE evaluation**: Efficiently Updatable Neural Networks for position evaluation with incremental accumulator updates during search.
- **Reinforcement learning**: self-play training cycle with supervised learning on game outcomes, model evaluation, and automatic model updates.
- **UCI protocol support**: works with GUIs such as CuteChess or Banksia by speaking the standard `uci/isready/position/go` command set. Search runs on a worker thread and supports both time and node limits.
- **Perft / benchmarking**: dedicated binary validates move-generation correctness and reports speed.

## Project Layout

```
├── bitboard.{h,cpp}      # Magic-bitboard helpers
├── board.{h,cpp}         # Bitboard representation + hashing helpers
├── engine.cpp            # UCI front-end and search control
├── evaluation.{h,cpp}    # Material + piece-square evaluation + NNUE integration
├── game.{h,cpp}          # Game state, legality checks, repetition
├── main.cpp              # Small regression tests
├── move.{h,cpp}          # Move encoding helpers
├── movetables.{h,cpp}    # Pre-generated king/knight/pawn moves + Zobrist
├── nnue.{h,cpp}          # NNUE network architecture and feature extraction
├── perft.cpp             # Perft driver
├── rl/                   # Reinforcement learning infrastructure
│   ├── rl_cycle.{h,cpp}  # Self-play training cycle manager
│   ├── self_play.{h,cpp} # Self-play game generation
│   ├── trainer.{h,cpp}   # NNUE training with SGD/Adam
│   ├── training_data.{h,cpp} # Training dataset management
│   ├── train_nnue.cpp    # Standalone training program
│   └── run_rl_cycle.cpp  # RL cycle runner
├── search.{h,cpp}        # Alpha-beta + quiescence search
├── testing/              # Engine testing framework
│   ├── test_runner.cpp   # Engine-vs-engine testing
│   ├── engine_interface.{h,cpp} # UCI engine communication
│   ├── game_runner.{h,cpp}     # Game execution
│   └── game_recorder.{h,cpp}   # Result recording
├── transposition.{h,cpp} # Zobrist TT
├── types.h               # Fundamental typedefs and constants
└── Makefile              # Build targets
```

## Building

### Prerequisites
- A C++17 compiler (tested with g++).
- `make`.

### Targets
```bash
make              # builds all targets
make engine       # UCI engine only
make perft        # perft driver only
make test_runner  # engine testing framework
make train_nnue   # standalone NNUE training program
make run_rl_cycle # reinforcement learning training cycle
make clean        # remove binaries/objects
```

`Makefile` defaults to `-O3 -std=c++17 -Wall -Wextra -pedantic`. Override `CXXFLAGS` on the command line if you need custom settings (e.g. `make CXXFLAGS='-std=c++20 -O2'`).

## Running the UCI Engine

Launch the binary and speak standard UCI commands (either manually or via a GUI):

```bash
./engine
uci
isready
position startpos moves e2e4 e7e5 g1f3
go wtime 300000 btime 300000
# ... wait for "bestmove" ...
stop        # optional early stop
quit
```

### Common commands
- `uci` – prints engine/author info and supported options (`Hash` size).
- `isready` – waits for the engine to finish any outstanding work and replies `readyok`.
- `ucinewgame` – reset internal state + clear TT.
- `position startpos | fen <...> [moves ...]` – set the current game.
- `go [wtime/btime/winc/binc/movetime/depth/nodes/searchmoves ...]` – start searching with time or node constraints. Search runs asynchronously; use `stop` to cut it off.
- `setoption name Hash value 256` – resize the transposition table (in MB) between searches.

### Using With a GUI
Point your GUI at the `engine` binary and let it handle the command exchange. Only the UCI commands above are required; unsupported commands print a human-readable info message instead of crashing.

### Evaluation Modes
The engine supports two evaluation modes:
- **Traditional**: material + piece-square tables (default)
- **NNUE**: neural network evaluation (set via `EVAL_MODE=nnue` environment variable)

To use NNUE evaluation, set the model path and mode:
```bash
export EVAL_MODE=nnue
export NNUE_MODEL=rl_model.bin
./engine
```

## Perft & Benchmarks

Build the perft tool (`make perft`) and run it directly:

```bash
./perft
```

By default it:
1. Verifies two standard positions (depth 5 & depth 4).
2. Runs a per-move breakdown for depth 4 from the KBNN vs rook test position.

Compare against the reference starting-position perft values:

| Depth | Nodes     |
|-------|-----------|
| 1     | 20        |
| 2     | 400       |
| 3     | 8,902     |
| 4     | 197,281   |
| 5     | 4,865,609 |

Modify `perft.cpp` to plug in custom FENs or depths when debugging move generation.

## Technical Notes

### Move Representation
- 16-bit packed value:
  - bits 0–5: `to` square
  - bits 6–11: `from` square
  - bits 12–15: move flags (quiet/double push/castles/captures/promotions)
- Extra bits encode captured piece IDs to simplify unmake.

### Board Representation
- Layered bitboards: one per piece type + color occupancy masks.
- Compact 16-bit `gameInfo` stores turn, castling rights, en-passant file, and half-move clock.
- Zobrist hashing via `MoveTables::zobrist*` seeds; hashing is updated incrementally on every make/unmake.

### Search
- Iterative-deepening alpha-beta with aspiration-style move ordering (hash move, captures, killer moves, center bias).
- Quiescence search for capture extensions.
- Killer-table and MVV-LVA heuristics.
- Transposition table with configurable size and three entry types (exact/lower/upper).
- Cooperative time control: node/time limits + async stop requests for responsive GUI interaction.

### NNUE Evaluation
- HalfKP feature set with incremental accumulator updates during move make/unmake.
- Network architecture: input (sparse features) → 256 → 32 → output (centipawns).
- Supports both traditional and NNUE evaluation modes with runtime switching.
- Model serialization for saving/loading trained networks.

### Reinforcement Learning
- Self-play game generation with configurable search depth and move limits.
- Supervised learning on game outcomes (win/draw/loss) with MSE loss.
- Training supports both SGD and Adam optimizers with configurable learning rates.
- Automatic model evaluation via engine-vs-engine games to determine improvements.
- RL cycle: self-play → collect data → train → evaluate → update model if better.

## NNUE Training

### Standalone Training
Train a model on existing data:
```bash
./train_nnue --dataset training_data.bin --model rl_model.bin --output trained_model.bin
```

Generate new training data from self-play:
```bash
./train_nnue --generate 100 --model rl_model.bin --output trained_model.bin
```

### Reinforcement Learning Cycle
Run the full RL training cycle (self-play → train → evaluate → update):
```bash
./run_rl_cycle --games 200 --epochs 10 --cycles 5 --model rl_model.bin
```

Options:
- `--games <n>`: number of self-play games per cycle (default: 100)
- `--epochs <n>`: training epochs per cycle (default: 10)
- `--cycles <n>`: maximum number of cycles (default: 10, 0 = infinite)
- `--eval-games <n>`: evaluation games between models (default: 20)
- `--model <path>`: model file path (default: rl_model.bin)
- `--threshold <f>`: win rate threshold to accept new model (default: 0.55)

The cycle automatically evaluates each new model against the previous version and only accepts improvements that exceed the threshold.

## Future Improvements

- [ ] Opening book + repetition-aware time management
- [ ] Endgame tablebases
- [ ] Advanced RL algorithms (PPO, TD-learning)
- [ ] Parallel search / lazy SMP
- [ ] SIMD optimizations for NNUE evaluation

## Lightweight Tests

`main.cpp` contains a handful of assertions covering special rules (castling, en passant, stalemate, promotions). Build/run it ad hoc if you are modifying core move generation:

```bash
g++ -std=c++17 -O2 main.cpp game.cpp board.cpp move.cpp movetables.cpp bitboard.cpp -o mini-tests
./mini-tests
```

---

**Built by [mackrabeau](https://github.com/mackrabeau)** — contributions and engine-vs-engine results are welcome!
