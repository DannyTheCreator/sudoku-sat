#include "sudoku.h"

#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition); return 0; \
} } while (0)

static const char *standard = "530070000600195000098000060800060003400803001700020006060000280000419005000080079";
static const char *answer = "534678912672195348198342567859761423426853791713924856961537284287419635345286179";

static SudokuBoard board_from(const char *text) {
    SudokuBoard board;
    for (int i = 0; i < 81; ++i) board.cells[i] = (unsigned char)(text[i] - '0');
    return board;
}

static int test_solver(void) {
    SatFormula formula;
    SatResult result;
    sat_formula_init(&formula, 0);
    sat_result_init(&result);
    CHECK(sat_solve(&formula, &result) == SAT_SAT);
    CHECK(sat_formula_is_satisfied(&formula, result.values));
    CHECK(sat_formula_add(&formula, NULL, 0));
    CHECK(sat_solve(&formula, &result) == SAT_UNSAT);
    sat_formula_free(&formula);

    sat_formula_init(&formula, 4);
    const int clauses[][3] = {{-1, 2, 0}, {-1, -2, 0}, {1, 3, 4}, {1, -3, 4}, {1, 3, -4}, {1, 3, 4}};
    for (size_t i = 0; i < 6; ++i) CHECK(sat_formula_add(&formula, clauses[i], i < 2 ? 2 : 3));
    CHECK(sat_solve(&formula, &result) == SAT_SAT);
    CHECK(result.values[1] == -1);
    CHECK(result.stats.conflicts > 0);
    CHECK(sat_formula_is_satisfied(&formula, result.values));
    CHECK(formula.clause_count == 6);
    CHECK(sat_solve(&formula, &result) == SAT_SAT);
    CHECK(sat_formula_is_satisfied(&formula, result.values));
    sat_formula_free(&formula);

    sat_formula_init(&formula, 2);
    const int impossible[][2] = {{1, 2}, {1, -2}, {-1, 2}, {-1, -2}};
    for (size_t i = 0; i < 4; ++i) CHECK(sat_formula_add(&formula, impossible[i], 2));
    CHECK(sat_solve(&formula, &result) == SAT_UNSAT);
    CHECK(result.values == NULL);
    CHECK(result.stats.conflicts >= 2);
    sat_formula_free(&formula);

    sat_formula_init(&formula, 6001);
    int large = 6001;
    CHECK(sat_formula_add(&formula, &large, 1));
    CHECK(sat_solve(&formula, &result) == SAT_SAT);
    CHECK(result.variable_count == 6001 && result.values[6001] == 1 && result.values[5000] == -1);
    CHECK(sat_formula_is_satisfied(&formula, result.values));
    sat_formula_free(&formula);

    sat_formula_init(&formula, 2);
    int duplicate[] = {1, 1, -2, -2}, tautology[] = {1, -1}, invalid[] = {0};
    CHECK(sat_formula_add(&formula, duplicate, 4));
    CHECK(sat_formula_add(&formula, tautology, 2));
    CHECK(!sat_formula_add(&formula, invalid, 1));
    CHECK(formula.clause_count == 1 && formula.clauses[0].count == 2);
    CHECK(duplicate[0] == 1 && duplicate[1] == 1 && duplicate[2] == -2);
    CHECK(sat_solve(&formula, &result) == SAT_SAT);
    sat_formula_free(&formula);
    sat_result_free(&result);
    return 1;
}

static int test_dimacs(const char *data) {
    SatFormula formula, reread;
    char path[2048], error[512];
    sat_formula_init(&formula, 0);
    sat_formula_init(&reread, 0);
    snprintf(path, sizeof(path), "%s/multiline.cnf", data);
    CHECK(sat_formula_read(&formula, path, error, sizeof(error)));
    CHECK(formula.variable_count == 4 && formula.clause_count == 3);
    CHECK(sat_formula_write(&formula, "test-roundtrip.cnf"));
    CHECK(sat_formula_read(&reread, "test-roundtrip.cnf", error, sizeof(error)));
    CHECK(reread.variable_count == formula.variable_count && reread.clause_count == formula.clause_count);
    for (size_t i = 0; i < formula.clause_count; ++i) {
        CHECK(reread.clauses[i].count == formula.clauses[i].count);
        CHECK(memcmp(reread.clauses[i].literals, formula.clauses[i].literals, formula.clauses[i].count * sizeof(int)) == 0);
    }
    CHECK(remove("test-roundtrip.cnf") == 0);
    const char *bad[] = {"bad-count.cnf", "bad-range.cnf", "bad-terminator.cnf", "bad-header.cnf", "bad-overflow.cnf", "bad-token.cnf", "missing.cnf"};
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
        snprintf(path, sizeof(path), "%s/%s", data, bad[i]);
        CHECK(!sat_formula_read(&formula, path, error, sizeof(error)));
        CHECK(error[0] != '\0');
        CHECK(formula.variable_count == 4 && formula.clause_count == 3);
    }
    sat_formula_free(&formula);
    sat_formula_free(&reread);
    return 1;
}

static int test_sudoku(void) {
    SudokuBoard board = board_from(standard), expected = board_from(answer), solved;
    SatFormula formula;
    sat_formula_init(&formula, 0);
    CHECK(sudoku_validate(&board, 0, 0));
    CHECK(!sudoku_validate(&board, 0, 1));
    CHECK(sudoku_solve(&board, 0, &solved, NULL) == SUDOKU_OK);
    CHECK(memcmp(&solved, &expected, sizeof(solved)) == 0);
    CHECK(solved.cells[80] == 9); /* Former off-by-one boundary at variable 729. */
    CHECK(sudoku_count_solutions(&board, 0, 2) == 1);
    CHECK(!sudoku_validate(&solved, 1, 1));
    board.cells[2] = 1; /* Legal local clue, incompatible with the unique completion. */
    CHECK(sudoku_validate(&board, 0, 0));
    CHECK(sudoku_solve(&board, 0, &solved, NULL) == SUDOKU_UNSAT);
    CHECK(sudoku_count_solutions(&board, 0, 2) == 0);
    board.cells[2] = 5;
    CHECK(!sudoku_validate(&board, 0, 0));
    CHECK(sudoku_solve(&board, 0, &solved, NULL) == SUDOKU_INVALID);
    board.cells[2] = 10;
    CHECK(!sudoku_validate(&board, 0, 0));

    SudokuBoard empty = {{0}};
    CHECK(sudoku_count_solutions(&empty, 0, 2) == 2);
    CHECK(sudoku_count_solutions(&empty, 0, 0) == -1);
    CHECK(sudoku_encode(&empty, 0, &formula) == SUDOKU_OK);
    CHECK(formula.variable_count == 729 && formula.clause_count == 11988);
    CHECK(sudoku_encode(&empty, 1, &formula) == SUDOKU_OK);
    CHECK(formula.clause_count == 12654);
    sat_formula_free(&formula);
    return 1;
}

static int empty_cells(const SudokuBoard *board) {
    int holes = 0;
    for (int i = 0; i < 81; ++i) if (!board->cells[i]) ++holes;
    return holes;
}

static int test_generator(void) {
    SudokuBoard puzzle, solution, replay, replay_solution, solved;
    const uint32_t seeds[] = {0, 42, 2026};
    for (int diagonal = 0; diagonal <= 1; ++diagonal) {
        for (size_t i = 0; i < 3; ++i) {
            CHECK(sudoku_generate(40, diagonal, seeds[i], &puzzle, &solution) == SUDOKU_OK);
            CHECK(empty_cells(&puzzle) == 40);
            CHECK(sudoku_validate(&solution, diagonal, 1));
            CHECK(sudoku_count_solutions(&puzzle, diagonal, 2) == 1);
            for (int cell = 0; cell < 81; ++cell) CHECK(!puzzle.cells[cell] || puzzle.cells[cell] == solution.cells[cell]);
            CHECK(sudoku_solve(&puzzle, diagonal, &solved, NULL) == SUDOKU_OK);
            CHECK(memcmp(&solved, &solution, sizeof(solved)) == 0);
            CHECK(sudoku_generate(40, diagonal, seeds[i], &replay, &replay_solution) == SUDOKU_OK);
            CHECK(memcmp(&replay, &puzzle, sizeof(puzzle)) == 0);
            CHECK(memcmp(&replay_solution, &solution, sizeof(solution)) == 0);
        }
    }
    CHECK(sudoku_generate(0, 0, 42, &puzzle, &solution) == SUDOKU_OK);
    CHECK(empty_cells(&puzzle) == 0);
    replay = puzzle;
    CHECK(sudoku_generate(-1, 0, 42, &puzzle, &solution) == SUDOKU_INVALID);
    CHECK(sudoku_generate(65, 0, 42, &puzzle, &solution) == SUDOKU_INVALID);
    CHECK(memcmp(&puzzle, &replay, sizeof(puzzle)) == 0);
    return 1;
}

int main(int argc, char **argv) {
    if (argc != 3) return EXIT_FAILURE;
    int passed = 0;
    if (strcmp(argv[1], "solver") == 0) passed = test_solver();
    else if (strcmp(argv[1], "dimacs") == 0) passed = test_dimacs(argv[2]);
    else if (strcmp(argv[1], "sudoku") == 0) passed = test_sudoku();
    else if (strcmp(argv[1], "generator") == 0) passed = test_generator();
    if (!passed) return EXIT_FAILURE;
    printf("PASS: %s\n", argv[1]);
    return EXIT_SUCCESS;
}
