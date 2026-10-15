# Implementation

**English** | [简体中文](ARCHITECTURE.zh-CN.md)

## SAT solver

A CNF formula is a dynamic array of clauses, each owning an array of integer literals. Solving leaves the formula unchanged, so it can be solved repeatedly. The solver context and result own their respective allocations.

Each nonempty clause has two watched positions. When an assignment makes a watched literal false, propagation visits only clauses watching that literal. It first tries to move the watch to another literal that is not false. If no replacement exists, the other watched literal produces a conflict when false or a unit assignment when unassigned. A unit clause registers one watch-list entry.

Every assignment is appended to a trail. A decision records the current trail position; a conflict rolls back all assignments after that checkpoint and tries the opposite polarity. If both polarities fail, search returns to the previous decision. The search loop is iterative, so SAT decision depth does not consume the C call stack.

Branch selection favors the shortest unsatisfied clause, using static, two-sided Jeroslow–Wang weights to choose among candidate variables. These weights are computed at initialization and are not updated during search. Once a model is found, unassigned variables are set to false and all retained clauses are checked again.

## Sudoku encoding

For zero-based row and column indices `r, c ∈ [0, 8]` and digit `d ∈ [1, 9]`:

```text
v = 81 × r + 9 × c + d
r = (v - 1) / 81
c = ((v - 1) % 81) / 9
d = (v - 1) % 9 + 1
```

Variables range from 1 to 729. Each of these nine-variable groups receives an exactly-one constraint: one nine-literal clause requiring at least one true variable, plus 36 pairwise exclusion clauses.

- The nine candidate digits in a cell.
- The nine positions for a digit in a row.
- The nine positions for a digit in a column.
- The nine positions for a digit in a 3×3 box.
- In diagonal mode, the nine positions for a digit on each main diagonal.

An empty standard board produces `324 × 37 = 11988` clauses. Diagonal mode adds `18 × 37 = 666`. Every clue adds one unit clause. The DIMACS header is derived from the formula, and solving does not require a prebuilt base CNF file.

## Solution counting and generation

A separate Sudoku searcher tracks occupied digits in rows, columns, boxes, and active diagonals using nine-bit masks. It selects an empty cell with the fewest candidates (MRV). Counting stops at the requested limit; uniqueness checks use a limit of two.

Generation uses a local xorshift32 state to shuffle candidate digits and the order of clue removal. It first constructs a valid complete board, then attempts to remove clues. A removal is retained only when the remaining puzzle still has exactly one solution. If a board cannot reach the requested number of blanks, generation starts another board, with a maximum of eight attempts.

The caller's puzzle and solution are updated only on success. The same seed, mode, hole count, and implementation version reproduce the same result.

## API conventions

Initialize `SatFormula` and `SatResult` with their respective `init` functions and release them with `free`. Adding a clause copies the caller's array. A failed file read preserves the existing formula; a successful read replaces it. `sat_solve` replaces the previous result. For SAT, `values[1..variable_count]` contains a complete model; UNSAT and ERROR results have no model.

In `SudokuBoard`, 0 means blank and 1–9 represent digits. File reading checks the character format and cell count; `sudoku_validate` checks the rules for the selected mode. Sudoku search has a maximum recursion depth of 81; SAT search is iterative.
