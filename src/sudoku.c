#include "sudoku.h"

#include <ctype.h>
#include <string.h>

typedef struct {
    SudokuBoard board;
    unsigned int rows[9], columns[9], boxes[9], diagonals[2];
    int diagonal;
} GridSearch;

static int box_id(int r, int c) { return (r / 3) * 3 + c / 3; }

static void mark(GridSearch *search, int cell, int digit, int add) {
    int r = cell / 9, c = cell % 9;
    unsigned int bit = 1U << (unsigned int)(digit - 1);
    if (add) {
        search->rows[r] |= bit;
        search->columns[c] |= bit;
        search->boxes[box_id(r, c)] |= bit;
        if (search->diagonal && r == c) search->diagonals[0] |= bit;
        if (search->diagonal && r + c == 8) search->diagonals[1] |= bit;
        search->board.cells[cell] = (unsigned char)digit;
    } else {
        search->rows[r] &= ~bit;
        search->columns[c] &= ~bit;
        search->boxes[box_id(r, c)] &= ~bit;
        if (search->diagonal && r == c) search->diagonals[0] &= ~bit;
        if (search->diagonal && r + c == 8) search->diagonals[1] &= ~bit;
        search->board.cells[cell] = 0;
    }
}

static unsigned int candidates(const GridSearch *search, int cell) {
    int r = cell / 9, c = cell % 9;
    unsigned int occupied = search->rows[r] | search->columns[c] | search->boxes[box_id(r, c)];
    if (search->diagonal && r == c) occupied |= search->diagonals[0];
    if (search->diagonal && r + c == 8) occupied |= search->diagonals[1];
    return (~occupied) & 0x1ffU;
}

static int grid_init(GridSearch *search, const SudokuBoard *board, int diagonal) {
    memset(search, 0, sizeof(*search));
    search->diagonal = diagonal;
    for (int i = 0; i < 81; ++i) {
        int digit = board->cells[i];
        if (digit > 9) return 0;
        if (digit) {
            if (!(candidates(search, i) & (1U << (unsigned int)(digit - 1)))) return 0;
            mark(search, i, digit, 1);
        }
    }
    return 1;
}

int sudoku_validate(const SudokuBoard *board, int diagonal, int require_complete) {
    GridSearch search;
    if (!grid_init(&search, board, diagonal)) return 0;
    if (require_complete) for (int i = 0; i < 81; ++i) if (!board->cells[i]) return 0;
    return 1;
}

int sudoku_read(SudokuBoard *board, const char *path, char *error, size_t error_size) {
    FILE *file = fopen(path, "r");
    if (!file) { snprintf(error, error_size, "Cannot open puzzle: %s", path); return 0; }
    SudokuBoard parsed = {{0}};
    int c, success = 1;
    size_t count = 0;
    while ((c = fgetc(file)) != EOF) {
        if (isspace((unsigned char)c)) continue;
        if (count >= 81 || (c != '.' && (c < '0' || c > '9'))) { success = 0; break; }
        parsed.cells[count++] = (unsigned char)(c == '.' ? 0 : c - '0');
    }
    if (ferror(file) || count != 81) success = 0;
    fclose(file);
    if (success) *board = parsed;
    else snprintf(error, error_size, "Expected exactly 81 cells: digits 1-9, 0 or .; whitespace is allowed");
    return success;
}

int sudoku_write(const SudokuBoard *board, const char *path) {
    FILE *file = fopen(path, "w");
    if (!file) return 0;
    for (int i = 0; i < 81; ++i) {
        fputc(board->cells[i] ? '0' + board->cells[i] : '.', file);
        if (i % 9 == 8) fputc('\n', file);
    }
    int success = !ferror(file);
    if (fclose(file) != 0) success = 0;
    return success;
}

void sudoku_print(const SudokuBoard *board, FILE *stream) {
    fputs("    1 2 3   4 5 6   7 8 9\n  +-------+-------+-------+\n", stream);
    for (int r = 0; r < 9; ++r) {
        fprintf(stream, "%d |", r + 1);
        for (int c = 0; c < 9; ++c) {
            int digit = board->cells[r * 9 + c];
            fprintf(stream, " %c", digit ? '0' + digit : '.');
            if (c % 3 == 2) fputs(" |", stream);
        }
        fputc('\n', stream);
        if (r % 3 == 2) fputs("  +-------+-------+-------+\n", stream);
    }
}

static int encoded(int r, int c, int digit) { return r * 81 + c * 9 + digit; }

static int exactly_one(SatFormula *formula, const int literals[9]) {
    if (!sat_formula_add(formula, literals, 9)) return 0;
    for (int i = 0; i < 9; ++i) {
        for (int j = i + 1; j < 9; ++j) {
            int pair[2] = {-literals[i], -literals[j]};
            if (!sat_formula_add(formula, pair, 2)) return 0;
        }
    }
    return 1;
}

SudokuStatus sudoku_encode(const SudokuBoard *board, int diagonal, SatFormula *formula) {
    if (!sudoku_validate(board, diagonal, 0)) return SUDOKU_INVALID;
    SatFormula built;
    sat_formula_init(&built, 729);
    int literals[9];
    for (int r = 0; r < 9; ++r) {
        for (int c = 0; c < 9; ++c) {
            for (int d = 1; d <= 9; ++d) literals[d - 1] = encoded(r, c, d);
            if (!exactly_one(&built, literals)) goto failed;
        }
    }
    for (int unit = 0; unit < 9; ++unit) {
        for (int d = 1; d <= 9; ++d) {
            for (int c = 0; c < 9; ++c) literals[c] = encoded(unit, c, d);
            if (!exactly_one(&built, literals)) goto failed;
            for (int r = 0; r < 9; ++r) literals[r] = encoded(r, unit, d);
            if (!exactly_one(&built, literals)) goto failed;
            for (int i = 0; i < 9; ++i) literals[i] = encoded((unit / 3) * 3 + i / 3, (unit % 3) * 3 + i % 3, d);
            if (!exactly_one(&built, literals)) goto failed;
        }
    }
    if (diagonal) {
        for (int d = 1; d <= 9; ++d) {
            for (int i = 0; i < 9; ++i) literals[i] = encoded(i, i, d);
            if (!exactly_one(&built, literals)) goto failed;
            for (int i = 0; i < 9; ++i) literals[i] = encoded(i, 8 - i, d);
            if (!exactly_one(&built, literals)) goto failed;
        }
    }
    for (int i = 0; i < 81; ++i) {
        if (board->cells[i]) {
            int literal = encoded(i / 9, i % 9, board->cells[i]);
            if (!sat_formula_add(&built, &literal, 1)) goto failed;
        }
    }
    sat_formula_free(formula);
    *formula = built;
    return SUDOKU_OK;
failed:
    sat_formula_free(&built);
    return SUDOKU_ERROR;
}

SudokuStatus sudoku_solve(const SudokuBoard *board, int diagonal, SudokuBoard *solution, SatStats *stats) {
    SatFormula formula;
    SatResult result;
    sat_formula_init(&formula, 0);
    sat_result_init(&result);
    SudokuStatus status = sudoku_encode(board, diagonal, &formula);
    if (stats) memset(stats, 0, sizeof(*stats));
    if (status != SUDOKU_OK) return status;
    SatStatus solved = sat_solve(&formula, &result);
    if (stats) *stats = result.stats;
    status = solved == SAT_SAT ? SUDOKU_OK : (solved == SAT_UNSAT ? SUDOKU_UNSAT : SUDOKU_ERROR);
    if (status == SUDOKU_OK) {
        SudokuBoard decoded = {{0}};
        for (int v = 1; v <= 729; ++v) {
            if (result.values[v] > 0) decoded.cells[(v - 1) / 9] = (unsigned char)((v - 1) % 9 + 1);
        }
        if (!sudoku_validate(&decoded, diagonal, 1)) status = SUDOKU_ERROR;
        else *solution = decoded;
    }
    sat_result_free(&result);
    sat_formula_free(&formula);
    return status;
}

static uint32_t random_next(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static int bit_count(unsigned int mask) {
    int count = 0;
    while (mask) { mask &= mask - 1; ++count; }
    return count;
}

/* MRV chooses a cell with the fewest candidates. Recursion is bounded by 81. */
static int search_grid(GridSearch *search, int limit, uint32_t *random, SudokuBoard *first) {
    int cell = -1, minimum = 10;
    unsigned int options = 0;
    for (int i = 0; i < 81; ++i) {
        if (!search->board.cells[i]) {
            unsigned int mask = candidates(search, i);
            int count = bit_count(mask);
            if (!count) return 0;
            if (count < minimum) { minimum = count; cell = i; options = mask; }
        }
    }
    if (cell < 0) { if (first) *first = search->board; return 1; }
    int digits[9], length = 0, total = 0;
    for (int d = 1; d <= 9; ++d) if (options & (1U << (unsigned int)(d - 1))) digits[length++] = d;
    if (random) {
        for (int i = length - 1; i > 0; --i) {
            int j = (int)(random_next(random) % (uint32_t)(i + 1));
            int temp = digits[i]; digits[i] = digits[j]; digits[j] = temp;
        }
    }
    for (int i = 0; i < length && total < limit; ++i) {
        mark(search, cell, digits[i], 1);
        total += search_grid(search, limit - total, random, first);
        mark(search, cell, digits[i], 0);
    }
    return total;
}

int sudoku_count_solutions(const SudokuBoard *board, int diagonal, int limit) {
    GridSearch search;
    if (limit < 1 || !grid_init(&search, board, diagonal)) return -1;
    return search_grid(&search, limit, NULL, NULL);
}

SudokuStatus sudoku_generate(int holes, int diagonal, uint32_t seed, SudokuBoard *puzzle, SudokuBoard *solution) {
    if (holes < 0 || holes > 64) return SUDOKU_INVALID;
    uint32_t state = seed ? seed : UINT32_C(0x9e3779b9);
    for (int attempt = 0; attempt < 8; ++attempt) {
        SudokuBoard empty = {{0}}, complete, candidate;
        GridSearch search;
        (void)grid_init(&search, &empty, diagonal);
        if (search_grid(&search, 1, &state, &complete) != 1) return SUDOKU_ERROR;
        candidate = complete;
        int order[81], removed = 0;
        for (int i = 0; i < 81; ++i) order[i] = i;
        for (int i = 80; i > 0; --i) {
            int j = (int)(random_next(&state) % (uint32_t)(i + 1));
            int temp = order[i]; order[i] = order[j]; order[j] = temp;
        }
        for (int i = 0; i < 81 && removed < holes; ++i) {
            int cell = order[i];
            unsigned char clue = candidate.cells[cell];
            candidate.cells[cell] = 0;
            if (sudoku_count_solutions(&candidate, diagonal, 2) == 1) ++removed;
            else candidate.cells[cell] = clue;
        }
        if (removed == holes) { *puzzle = candidate; *solution = complete; return SUDOKU_OK; }
    }
    return SUDOKU_UNSAT; /* The requested removal target was not reached. */
}
