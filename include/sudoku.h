#ifndef SUDOKU_BOARD_H
#define SUDOKU_BOARD_H

#include "sat.h"
#include <stdint.h>
#include <stdio.h>

enum { SUDOKU_SIDE = 9, SUDOKU_CELLS = 81, SUDOKU_VARIABLES = 729 };
typedef struct { unsigned char cells[SUDOKU_CELLS]; } SudokuBoard;
typedef enum { SUDOKU_ERROR = -2, SUDOKU_INVALID = -1, SUDOKU_UNSAT = 0, SUDOKU_OK = 1 } SudokuStatus;

int sudoku_read(SudokuBoard *board, const char *path, char *error, size_t error_size);
int sudoku_write(const SudokuBoard *board, const char *path);
void sudoku_print(const SudokuBoard *board, FILE *stream);
int sudoku_validate(const SudokuBoard *board, int diagonal, int require_complete);
SudokuStatus sudoku_encode(const SudokuBoard *board, int diagonal, SatFormula *formula);
SudokuStatus sudoku_solve(const SudokuBoard *board, int diagonal, SudokuBoard *solution, SatStats *stats);
/* Returns the count capped at limit, or -1 for an invalid board/limit. */
int sudoku_count_solutions(const SudokuBoard *board, int diagonal, int limit);
/* Try at most 8 complete grids; output boards are changed only on success. */
SudokuStatus sudoku_generate(int holes, int diagonal, uint32_t seed, SudokuBoard *puzzle, SudokuBoard *solution);

#endif
