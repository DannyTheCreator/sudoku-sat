#ifndef SUDOKU_SAT_H
#define SUDOKU_SAT_H

#include <stddef.h>

typedef struct {
    int *literals;
    size_t count;
} SatClause;

typedef struct {
    int variable_count;
    SatClause *clauses;
    size_t clause_count;
    size_t capacity;
} SatFormula;

typedef enum { SAT_ERROR = -1, SAT_UNSAT = 0, SAT_SAT = 1 } SatStatus;

typedef struct {
    size_t decisions;
    size_t propagations;
    size_t conflicts;
    double milliseconds;
} SatStats;

typedef struct {
    SatStatus status;
    int variable_count;
    signed char *values; /* Indexed from 1: -1 = false, 1 = true. */
    SatStats stats;
} SatResult;

/* Initialize objects before use. Free functions also reset them. */
void sat_formula_init(SatFormula *formula, int variable_count);
void sat_formula_free(SatFormula *formula);
int sat_formula_add(SatFormula *formula, const int *literals, size_t count);
int sat_formula_read(SatFormula *formula, const char *path, char *error, size_t error_size);
int sat_formula_write(const SatFormula *formula, const char *path);
int sat_formula_is_satisfied(const SatFormula *formula, const signed char *values);

void sat_result_init(SatResult *result);
void sat_result_free(SatResult *result);
SatStatus sat_solve(const SatFormula *formula, SatResult *result);
int sat_result_write(const SatResult *result, const char *path);

#endif
