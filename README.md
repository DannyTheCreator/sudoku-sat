# Sudoku SAT

**English** | [简体中文](README.zh-CN.md)

[![Build and test](https://github.com/DannyTheCreator/sudoku-sat/actions/workflows/ci.yml/badge.svg)](https://github.com/DannyTheCreator/sudoku-sat/actions/workflows/ci.yml)

A portable C11 SAT solver and Sudoku toolkit with standard and diagonal Sudoku solving, unique puzzle generation, and an interactive terminal game.

The SAT solver implements DPLL with two watched literals, unit propagation, and assignment-trail backtracking. Sudoku puzzles are encoded as CNF formulas over 729 Boolean variables. The executable uses only the C standard library.

[Quick start](#quick-start) · [Commands](#commands) · [File formats](#file-formats) · [Testing](#testing) · [Implementation](docs/ARCHITECTURE.md)

## Features

| Feature | Description |
| --- | --- |
| SAT solving | Read DIMACS CNF and report SAT or UNSAT, complete assignments, and solver statistics |
| Sudoku solving | Solve standard 9×9 puzzles or Sudoku X with both main diagonals constrained |
| CNF export | Export a puzzle as DIMACS CNF for inspection or use with another solver |
| Unique puzzle generation | Check uniqueness after each removed clue; reproduce puzzles with a fixed seed |
| Terminal game | Enter or clear values, request hints, show a solution, and check whether current entries can be completed |
| Build and tests | CMake, CTest, and GitHub Actions for Windows, Linux, and macOS |

## Quick start

Requirements:

- A compiler with C11 support, such as GCC, Clang, or a recent MSVC toolchain.
- CMake 3.21 or later.
- Python 3.8 or later for optional CLI and differential tests. Python is not needed to run the executable.

Run these commands from the project root.

### Linux / macOS

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/sudoku solve examples/standard.txt --check-unique
```

### Windows / MinGW

Ensure `gcc`, `mingw32-make`, and `cmake` are available on `PATH`, then open PowerShell:

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
.\build\sudoku.exe solve examples\standard.txt --check-unique
```

### Windows / Visual Studio

Use a Visual Studio developer terminal with the C/C++ toolchain installed:

```powershell
cmake -S . -B build
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
.\build\Release\sudoku.exe solve examples\standard.txt --check-unique
```

Use a separate build directory, such as `build-msvc`, when switching generators.

The examples below use `./build/sudoku`; substitute the appropriate `.exe` path on Windows. Running the executable without arguments opens the interactive menu. Use `--help` for the command reference.

## Commands

### Solve SAT formulas

```bash
./build/sudoku sat examples/sat.cnf -o answer.res
./build/sudoku sat examples/unsat.cnf -o unsat.res
```

The command reports satisfiability and CPU time, decisions, propagations, and conflicts. `-o` saves a result file. UNSAT is a normal solver result and returns exit code 2.

### Solve Sudoku puzzles

```bash
./build/sudoku solve examples/standard.txt --check-unique -o solved.txt
./build/sudoku solve examples/diagonal.txt --diagonal --check-unique
```

`--check-unique` reports `unique` or `multiple`. A solvable puzzle with multiple solutions still returns success. In diagonal mode, both length-nine main diagonals must contain each digit exactly once; the center cell belongs to both diagonals.

### Export CNF

```bash
./build/sudoku encode examples/standard.txt puzzle.cnf
./build/sudoku encode examples/diagonal.txt diagonal.cnf --diagonal
./build/sudoku sat puzzle.cnf -o puzzle.res
```

### Generate a unique puzzle

```bash
./build/sudoku generate --holes 40 --seed 42 -o generated.txt --solution solution.txt
./build/sudoku generate --holes 50 --seed 42 --diagonal -o generated-x.txt
```

The default is 40 blank cells and a time-based seed. `--holes` accepts 0–64; `--seed` accepts 0–4294967295. Seed 0 maps to a fixed nonzero internal state. The same seed, mode, hole count, and implementation version reproduce the same result.

The generator tries at most eight complete grids. On success, the puzzle has exactly the requested number of blanks and one solution. If the target cannot be reached, it returns exit code 2 without writing an incomplete result. High hole counts may fail; the number of blanks is not a difficulty rating.

### Play in the terminal

```bash
./build/sudoku play examples/standard.txt
./build/sudoku play examples/diagonal.txt --diagonal
```

Rows and columns use **1–9**:

| Command | Action |
| --- | --- |
| `set 1 3 4` | Set row 1, column 3 to 4 |
| `clear 1 3` | Clear that cell |
| `hint 1 3` | Fill that cell from one valid solution |
| `check` | Check whether the current entries can be completed |
| `answer` | Display one valid solution |
| `quit` | Exit the game |

Original clues are locked. Moves that conflict with the current row, column, box, or active diagonals are rejected. For puzzles with multiple solutions, hints follow one valid solution; completion accepts any full board that satisfies the rules.

The interactive menu also supports saving solver results, selecting either Sudoku mode, saving generated puzzles, and immediately starting a game.

## File formats

### Sudoku

A puzzle contains exactly 81 cells. Use `1`–`9` for clues and `.` or `0` for blanks. Whitespace is ignored:

```text
53..7....
6..195...
.98....6.
8...6...3
4..8.3..1
7...2...6
.6....28.
...419..5
....8..79
```

### DIMACS CNF

```text
c A small satisfiable formula
p cnf 3 3
1 2 0
-1 3 0
-2 -3 0
```

The header declares the variable and clause counts. Each clause ends with `0` and may span lines. Lines starting with `c`, optionally preceded by whitespace, are comments. The reader checks clause counts, variable ranges, integer overflow, and missing terminators. Duplicate literals are merged and tautological clauses are discarded after validating the original clause count.

### SAT results

```text
s 1
v 1 -2 3 0
t 0.000
```

`s 1` means SAT; `s 0` means UNSAT. SAT results include a complete model on the `v` line, terminated by `0`. Unconstrained variables are assigned false. `t` records solver CPU time in milliseconds.

Output paths are used as supplied. Existing output files are overwritten, and parent directories must already exist.

| Exit code | Meaning |
| --- | --- |
| 0 | Success |
| 1 | Invalid arguments, malformed input, conflicting clues, I/O failure, or an internal error |
| 2 | UNSAT, an unsolvable puzzle, or an unmet generation target |

## Project structure

```text
.
├── include/                 Public SAT and Sudoku interfaces
├── src/
│   ├── sat.c                DPLL, watched literals, and assignment trails
│   ├── dimacs.c             CNF and result-file I/O
│   ├── sudoku.c             Encoding, solving, solution counting, and generation
│   └── main.c               CLI and interactive terminal interface
├── examples/                Sudoku puzzles and SAT / UNSAT formulas
├── tests/                   C regression tests and a Python brute-force oracle
├── docs/                    Implementation notes in English and Chinese
├── .github/                 CI workflows and issue / pull request templates
├── README.md                English documentation
└── README.zh-CN.md          Chinese documentation
```

## Testing

CTest runs four C suites. When Python is available, it also runs CLI integration tests and compares 250 deterministic random SAT formulas against exhaustive enumeration. Coverage includes backtracking, empty formulas and clauses, dynamic variable counts, malformed CNF, model decoding, both Sudoku modes, uniqueness, and seed reproducibility.

Treat compiler warnings as errors:

```bash
cmake -S . -B build-strict -DCMAKE_BUILD_TYPE=Release -DWARNINGS_AS_ERRORS=ON
cmake --build build-strict --parallel
ctest --test-dir build-strict --output-on-failure
```

Run AddressSanitizer and UndefinedBehaviorSanitizer with a supported GCC / Clang toolchain:

```bash
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON
cmake --build build-asan --parallel
ctest --test-dir build-asan --output-on-failure
```

GitHub Actions runs Windows, Linux, and macOS builds and tests on pushes and pull requests, plus memory and undefined-behavior checks on Linux.

## Implementation and scope

Sudoku solving uses CNF encoding and the DPLL solver. Puzzle generation and solution counting use bit masks and minimum-remaining-values (MRV) search. See the [implementation notes](docs/ARCHITECTURE.md) for the constraints, backtracking behavior, and API ownership rules.

The solver is intended for algorithm study and small to medium SAT problems. It does not implement CDCL, clause learning, or restarts; large, difficult instances may take substantial time.
