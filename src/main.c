#include "sudoku.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum { OPT_OUTPUT = 1, OPT_DIAGONAL = 2, OPT_UNIQUE = 4, OPT_HOLES = 8, OPT_SEED = 16, OPT_SOLUTION = 32 };
typedef struct {
    int diagonal, unique, holes;
    uint32_t seed;
    const char *output, *solution;
} Options;

static void usage(void) {
    puts("Sudoku SAT - C11 SAT solver and Sudoku toolkit\n"
         "Usage:\n"
         "  sudoku                         Interactive menu\n"
         "  sudoku sat INPUT.cnf [-o RESULT.res]\n"
         "  sudoku solve PUZZLE.txt [--diagonal] [--check-unique] [-o SOLVED.txt]\n"
         "  sudoku encode PUZZLE.txt OUTPUT.cnf [--diagonal]\n"
         "  sudoku generate [--holes 0..64] [--seed UINT32] [--diagonal]\n"
         "                  [-o PUZZLE.txt] [--solution SOLVED.txt]\n"
         "  sudoku play PUZZLE.txt [--diagonal]\n"
         "  sudoku --help\n\n"
         "Puzzle format: 81 cells, digits 1-9; . or 0 for blanks. Whitespace allowed.\n"
         "Exit codes: 0 = success, 1 = input/I/O/internal error, 2 = no solution\n"
         "or generation target not reached. Generation defaults to 40 holes.\n"
         "Existing output files are overwritten when an output path is supplied.");
}

static int integer(const char *text, int low, int high, int *value) {
    char *end;
    errno = 0;
    long number = strtol(text, &end, 10);
    if (errno || end == text || *end || number < low || number > high) return 0;
    *value = (int)number;
    return 1;
}

static int parse_options(int argc, char **argv, int start, unsigned int allowed, Options *options) {
    *options = (Options){0, 0, 40, (uint32_t)time(NULL), NULL, NULL};
    for (int i = start; i < argc; ++i) {
        unsigned int flag = 0;
        if (strcmp(argv[i], "--diagonal") == 0) flag = OPT_DIAGONAL;
        else if (strcmp(argv[i], "--check-unique") == 0) flag = OPT_UNIQUE;
        else if (strcmp(argv[i], "--holes") == 0) flag = OPT_HOLES;
        else if (strcmp(argv[i], "--seed") == 0) flag = OPT_SEED;
        else if (strcmp(argv[i], "--solution") == 0) flag = OPT_SOLUTION;
        else if (strcmp(argv[i], "-o") == 0) flag = OPT_OUTPUT;
        if (!flag || !(flag & allowed)) { fprintf(stderr, "Unsupported option: %s\n", argv[i]); return 0; }
        if (flag == OPT_DIAGONAL) options->diagonal = 1;
        else if (flag == OPT_UNIQUE) options->unique = 1;
        else {
            if (++i >= argc) { fputs("Missing option value.\n", stderr); return 0; }
            if (flag == OPT_OUTPUT) options->output = argv[i];
            else if (flag == OPT_SOLUTION) options->solution = argv[i];
            else if (flag == OPT_HOLES) {
                if (!integer(argv[i], 0, 64, &options->holes)) { fputs("Holes must be between 0 and 64.\n", stderr); return 0; }
            } else {
                char *end;
                errno = 0;
                unsigned long long seed = strtoull(argv[i], &end, 10);
                if (errno || end == argv[i] || argv[i][0] == '-' || *end || seed > UINT32_MAX) {
                    fputs("Seed must be an unsigned 32-bit integer.\n", stderr); return 0;
                }
                options->seed = (uint32_t)seed;
            }
        }
    }
    return 1;
}

static void print_stats(const SatStats *stats) {
    printf("CPU time: %.3f ms | decisions: %zu | propagations: %zu | conflicts: %zu\n",
           stats->milliseconds, stats->decisions, stats->propagations, stats->conflicts);
}

static int load_board(const char *path, int diagonal, SudokuBoard *board) {
    char error[512];
    if (!sudoku_read(board, path, error, sizeof(error))) { fprintf(stderr, "%s\n", error); return 0; }
    if (!sudoku_validate(board, diagonal, 0)) { fputs("Conflicting clues in the selected Sudoku mode.\n", stderr); return 0; }
    return 1;
}

static int save_board(const SudokuBoard *board, const char *path) {
    if (!path) return 1;
    if (!sudoku_write(board, path)) { fprintf(stderr, "Cannot write puzzle: %s\n", path); return 0; }
    printf("Saved: %s\n", path);
    return 1;
}

static int run_sat(int argc, char **argv) {
    Options options;
    if (argc < 3 || !parse_options(argc, argv, 3, OPT_OUTPUT, &options)) return 1;
    SatFormula formula;
    SatResult result;
    char error[512];
    sat_formula_init(&formula, 0);
    sat_result_init(&result);
    if (!sat_formula_read(&formula, argv[2], error, sizeof(error))) { fprintf(stderr, "%s\n", error); return 1; }
    SatStatus status = sat_solve(&formula, &result);
    int exit_code = status == SAT_SAT ? 0 : (status == SAT_UNSAT ? 2 : 1);
    if (status == SAT_ERROR) fputs("SAT solver failed (memory allocation or internal error).\n", stderr);
    else {
        puts(status == SAT_SAT ? "SATISFIABLE" : "UNSATISFIABLE");
        print_stats(&result.stats);
        if (options.output && !sat_result_write(&result, options.output)) {
            fprintf(stderr, "Cannot write result: %s\n", options.output);
            exit_code = 1;
        }
    }
    sat_result_free(&result);
    sat_formula_free(&formula);
    return exit_code;
}

static int run_solve(int argc, char **argv) {
    Options options;
    SudokuBoard board, solution;
    SatStats stats;
    if (argc < 3 || !parse_options(argc, argv, 3, OPT_OUTPUT | OPT_DIAGONAL | OPT_UNIQUE, &options)) return 1;
    if (!load_board(argv[2], options.diagonal, &board)) return 1;
    SudokuStatus status = sudoku_solve(&board, options.diagonal, &solution, &stats);
    if (status != SUDOKU_OK) { fputs(status == SUDOKU_UNSAT ? "No solution.\n" : "Unable to solve puzzle.\n", stderr); return status == SUDOKU_UNSAT ? 2 : 1; }
    sudoku_print(&solution, stdout);
    print_stats(&stats);
    if (options.unique) printf("Solutions: %s\n", sudoku_count_solutions(&board, options.diagonal, 2) == 1 ? "unique" : "multiple");
    return save_board(&solution, options.output) ? 0 : 1;
}

static int run_encode(int argc, char **argv) {
    Options options;
    SudokuBoard board;
    if (argc < 4 || !parse_options(argc, argv, 4, OPT_DIAGONAL, &options)) return 1;
    if (!load_board(argv[2], options.diagonal, &board)) return 1;
    SatFormula formula;
    sat_formula_init(&formula, 0);
    SudokuStatus status = sudoku_encode(&board, options.diagonal, &formula);
    int success = status == SUDOKU_OK && sat_formula_write(&formula, argv[3]);
    if (success) printf("Encoded %d variables and %zu clauses: %s\n", formula.variable_count, formula.clause_count, argv[3]);
    else fputs("Unable to encode or write CNF file.\n", stderr);
    sat_formula_free(&formula);
    return success ? 0 : 1;
}

static int run_generate(int argc, char **argv) {
    Options options;
    SudokuBoard puzzle, solution;
    if (!parse_options(argc, argv, 2, OPT_OUTPUT | OPT_DIAGONAL | OPT_HOLES | OPT_SEED | OPT_SOLUTION, &options)) return 1;
    if (options.output && options.solution && strcmp(options.output, options.solution) == 0) {
        fputs("Puzzle and solution must use different output paths.\n", stderr); return 1;
    }
    SudokuStatus status = sudoku_generate(options.holes, options.diagonal, options.seed, &puzzle, &solution);
    if (status != SUDOKU_OK) {
        fputs(status == SUDOKU_UNSAT ? "Could not reach the hole target with a unique solution. Try fewer holes or a different seed.\n" : "Puzzle generation failed.\n", stderr);
        return status == SUDOKU_UNSAT ? 2 : 1;
    }
    printf("Unique %s puzzle | holes: %d | seed: %lu\n", options.diagonal ? "diagonal" : "standard", options.holes, (unsigned long)options.seed);
    sudoku_print(&puzzle, stdout);
    return save_board(&puzzle, options.output) && save_board(&solution, options.solution) ? 0 : 1;
}

static int read_line(const char *prompt, char *line, size_t capacity) {
    fputs(prompt, stdout);
    fflush(stdout);
    if (!fgets(line, (int)capacity, stdin)) return 0;
    size_t length = strlen(line);
    if (length && line[length - 1] != '\n' && !feof(stdin)) {
        int c;
        while ((c = getchar()) != '\n' && c != EOF) {}
        fputs("Input line is too long.\n", stderr);
        return -1;
    }
    while (length && (line[length - 1] == '\n' || line[length - 1] == '\r')) line[--length] = '\0';
    return 1;
}

static int run_play(int argc, char **argv) {
    Options options;
    SudokuBoard clues, board, solution;
    if (argc < 3 || !parse_options(argc, argv, 3, OPT_DIAGONAL, &options)) return 1;
    if (!load_board(argv[2], options.diagonal, &clues)) return 1;
    SudokuStatus status = sudoku_solve(&clues, options.diagonal, &solution, NULL);
    if (status != SUDOKU_OK) { fputs("This puzzle cannot be played: no solution or solver failure.\n", stderr); return status == SUDOKU_UNSAT ? 2 : 1; }
    board = clues;
    puts("Commands (row/column 1-9): set R C D, clear R C, hint R C, answer, check, quit.\n"
         "Original clues are locked. Hints and answer follow one valid solution.");
    for (;;) {
        sudoku_print(&board, stdout);
        if (sudoku_validate(&board, options.diagonal, 1)) { puts("Puzzle complete. Well done!"); return 0; }
        char line[256], *tokens[5];
        int read = read_line("play> ", line, sizeof(line));
        if (!read) return 0;
        if (read < 0) continue;
        int count = 0;
        char *token = strtok(line, " \t");
        while (token && count < 5) { tokens[count++] = token; token = strtok(NULL, " \t"); }
        if (!count) continue;
        if (count == 1 && strcmp(tokens[0], "quit") == 0) return 0;
        if (count == 1 && strcmp(tokens[0], "answer") == 0) { sudoku_print(&solution, stdout); continue; }
        if (count == 1 && strcmp(tokens[0], "check") == 0) {
            puts(sudoku_count_solutions(&board, options.diagonal, 1) == 1 ? "The current entries can be completed." : "The current entries lead to a dead end.");
            continue;
        }
        int is_set = strcmp(tokens[0], "set") == 0;
        int is_clear = strcmp(tokens[0], "clear") == 0;
        int is_hint = strcmp(tokens[0], "hint") == 0;
        int r, c, digit = 0;
        if ((!is_set && !is_clear && !is_hint) || count != (is_set ? 4 : 3) ||
            !integer(tokens[1], 1, 9, &r) || !integer(tokens[2], 1, 9, &c) ||
            (is_set && !integer(tokens[3], 1, 9, &digit))) {
            puts("Invalid command. Use set R C D, clear R C, hint R C, answer, check or quit."); continue;
        }
        int cell = (r - 1) * 9 + c - 1;
        if (clues.cells[cell]) { puts("This cell is an original clue."); continue; }
        unsigned char old = board.cells[cell];
        board.cells[cell] = (unsigned char)(is_clear ? 0 : (is_hint ? solution.cells[cell] : digit));
        if (!sudoku_validate(&board, options.diagonal, 0)) { board.cells[cell] = old; puts("Move conflicts with the current entries."); }
    }
}

static int interactive(void) {
    char choice[32], path[1024], output[1024], extra[32], holes[32];
    for (;;) {
        puts("\nSudoku SAT\n  1. Solve a DIMACS CNF\n  2. Solve a Sudoku\n  3. Generate a unique Sudoku\n  4. Play a Sudoku\n  0. Exit");
        int read = read_line("Choice: ", choice, sizeof(choice)), selected;
        if (!read) return 0;
        if (read < 0 || !integer(choice, 0, 4, &selected)) { puts("Choose 0-4."); continue; }
        if (!selected) return 0;
        if (selected == 3) {
            if (read_line("Holes (0-64, default 40): ", holes, sizeof(holes)) != 1) continue;
            if (!holes[0]) strcpy(holes, "40");
            if (read_line("Diagonal mode? (y/N): ", extra, sizeof(extra)) != 1) continue;
            int diagonal = extra[0] == 'y' || extra[0] == 'Y';
            if (read_line("Save puzzle as (default generated.txt): ", path, sizeof(path)) != 1) continue;
            if (!path[0]) strcpy(path, "generated.txt");
            char *args[7] = {"sudoku", "generate", "--holes", holes};
            int arg_count = 4;
            if (diagonal) args[arg_count++] = "--diagonal";
            args[arg_count++] = "-o";
            args[arg_count++] = path;
            if (run_generate(arg_count, args) == 0) {
                if (read_line("Play the generated puzzle now? (y/N): ", extra, sizeof(extra)) != 1) continue;
                if (extra[0] == 'y' || extra[0] == 'Y') {
                    char *play_args[] = {"sudoku", "play", path, "--diagonal"};
                    (void)run_play(diagonal ? 4 : 3, play_args);
                }
            }
        } else {
            if (read_line("Input file path: ", path, sizeof(path)) != 1) continue;
            char *args[6] = {"sudoku", selected == 1 ? "sat" : (selected == 2 ? "solve" : "play"), path};
            int arg_count = 3;
            if (selected != 1) {
                if (read_line("Diagonal mode? (y/N): ", extra, sizeof(extra)) != 1) continue;
                if (extra[0] == 'y' || extra[0] == 'Y') args[arg_count++] = "--diagonal";
            }
            if (selected != 4) {
                if (read_line("Output file path (blank to skip): ", output, sizeof(output)) != 1) continue;
                if (output[0]) { args[arg_count++] = "-o"; args[arg_count++] = output; }
            }
            if (selected == 1) (void)run_sat(arg_count, args);
            else if (selected == 2) (void)run_solve(arg_count, args);
            else (void)run_play(arg_count, args);
        }
    }
}

int main(int argc, char **argv) {
    if (argc == 1) return interactive();
    if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) { usage(); return 0; }
    if (strcmp(argv[1], "sat") == 0) { if (argc < 3) usage(); return run_sat(argc, argv); }
    if (strcmp(argv[1], "solve") == 0) { if (argc < 3) usage(); return run_solve(argc, argv); }
    if (strcmp(argv[1], "encode") == 0) { if (argc < 4) usage(); return run_encode(argc, argv); }
    if (strcmp(argv[1], "generate") == 0) return run_generate(argc, argv);
    if (strcmp(argv[1], "play") == 0) { if (argc < 3) usage(); return run_play(argc, argv); }
    fprintf(stderr, "Unknown command: %s\n", argv[1]);
    usage();
    return 1;
}
