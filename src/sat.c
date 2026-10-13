#include "sat.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct { size_t *items, count, capacity; } WatchList;
typedef struct { size_t positions[2]; } WatchedClause;
typedef struct { int literal, flipped; size_t checkpoint; } Decision;
typedef struct {
    const SatFormula *formula;
    signed char *values;
    int *trail;
    size_t trail_count, queue_head;
    Decision *decisions;
    size_t decision_count;
    WatchedClause *clauses;
    WatchList *watches;
    double *positive, *negative;
    SatStats stats;
} Solver;

static int variable(int literal) { return literal < 0 ? -literal : literal; }
static size_t literal_index(int literal) {
    return 2 * ((size_t)variable(literal) - 1) + (size_t)(literal < 0);
}

void sat_formula_init(SatFormula *formula, int variable_count) {
    memset(formula, 0, sizeof(*formula));
    formula->variable_count = variable_count;
}

void sat_formula_free(SatFormula *formula) {
    for (size_t i = 0; i < formula->clause_count; ++i) free(formula->clauses[i].literals);
    free(formula->clauses);
    sat_formula_init(formula, 0);
}

static int compare_literals(const void *left, const void *right) {
    int a = *(const int *)left, b = *(const int *)right;
    int av = variable(a), bv = variable(b);
    return av != bv ? (av > bv) - (av < bv) : (a > b) - (a < b);
}

int sat_formula_add(SatFormula *formula, const int *literals, size_t count) {
    int *copy = NULL;
    size_t used = 0;
    if (formula->variable_count < 0 || count > SIZE_MAX / sizeof(int) || (count && !literals)) return 0;
    for (size_t i = 0; i < count; ++i) {
        if (literals[i] == 0 || literals[i] == INT_MIN || variable(literals[i]) > formula->variable_count) return 0;
    }
    if (count) {
        copy = malloc(count * sizeof(*copy));
        if (!copy) return 0;
        memcpy(copy, literals, count * sizeof(*copy));
        qsort(copy, count, sizeof(*copy), compare_literals);
        for (size_t i = 0; i < count; ++i) {
            if (used && copy[i] == copy[used - 1]) continue;
            if (used && variable(copy[i]) == variable(copy[used - 1])) {
                free(copy); /* A tautology imposes no constraint. */
                return 1;
            }
            copy[used++] = copy[i];
        }
    }
    if (formula->clause_count == formula->capacity) {
        size_t capacity = formula->capacity ? formula->capacity * 2 : 16;
        if (capacity < formula->capacity || capacity > SIZE_MAX / sizeof(SatClause)) { free(copy); return 0; }
        SatClause *clauses = realloc(formula->clauses, capacity * sizeof(*clauses));
        if (!clauses) { free(copy); return 0; }
        formula->clauses = clauses;
        formula->capacity = capacity;
    }
    formula->clauses[formula->clause_count++] = (SatClause){copy, used};
    return 1;
}

int sat_formula_is_satisfied(const SatFormula *formula, const signed char *values) {
    for (size_t i = 0; i < formula->clause_count; ++i) {
        int satisfied = 0;
        for (size_t j = 0; j < formula->clauses[i].count; ++j) {
            int literal = formula->clauses[i].literals[j];
            if (values[variable(literal)] == (literal > 0 ? 1 : -1)) { satisfied = 1; break; }
        }
        if (!satisfied) return 0;
    }
    return 1;
}

void sat_result_init(SatResult *result) {
    memset(result, 0, sizeof(*result));
    result->status = SAT_ERROR;
}

void sat_result_free(SatResult *result) {
    free(result->values);
    sat_result_init(result);
}

static int watch_push(WatchList *list, size_t clause) {
    if (list->count == list->capacity) {
        size_t capacity = list->capacity ? list->capacity * 2 : 4;
        if (capacity < list->capacity || capacity > SIZE_MAX / sizeof(size_t)) return 0;
        size_t *items = realloc(list->items, capacity * sizeof(*items));
        if (!items) return 0;
        list->items = items;
        list->capacity = capacity;
    }
    list->items[list->count++] = clause;
    return 1;
}

static int literal_value(const Solver *solver, int literal) {
    int value = solver->values[variable(literal)];
    return literal > 0 ? value : -value;
}

static int assign_literal(Solver *solver, int literal) {
    int value = literal_value(solver, literal);
    if (value) return value > 0;
    solver->values[variable(literal)] = (signed char)(literal > 0 ? 1 : -1);
    solver->trail[solver->trail_count++] = literal;
    return 1;
}

/* 1 = success, 0 = conflict, -1 = allocation failure. */
static int propagate(Solver *solver) {
    while (solver->queue_head < solver->trail_count) {
        int falsified = -solver->trail[solver->queue_head++];
        ++solver->stats.propagations;
        WatchList *list = &solver->watches[literal_index(falsified)];
        size_t i = 0;
        while (i < list->count) {
            size_t id = list->items[i];
            const SatClause *clause = &solver->formula->clauses[id];
            WatchedClause *watched = &solver->clauses[id];
            size_t slot = (size_t)(clause->literals[watched->positions[0]] != falsified);
            int other = clause->literals[watched->positions[1 - slot]];
            if (literal_value(solver, other) > 0) { ++i; continue; }
            size_t replacement = clause->count;
            for (size_t j = 0; j < clause->count; ++j) {
                if (j != watched->positions[0] && j != watched->positions[1] && literal_value(solver, clause->literals[j]) >= 0) {
                    replacement = j;
                    break;
                }
            }
            if (replacement < clause->count) {
                if (!watch_push(&solver->watches[literal_index(clause->literals[replacement])], id)) return -1;
                watched->positions[slot] = replacement;
                list->items[i] = list->items[--list->count];
            } else {
                if (!assign_literal(solver, other)) return 0;
                ++i;
            }
        }
    }
    return 1;
}

static int choose_literal(const Solver *solver) {
    size_t shortest = SIZE_MAX;
    double best_score = -1.0;
    int choice = 0;
    for (size_t i = 0; i < solver->formula->clause_count; ++i) {
        const SatClause *clause = &solver->formula->clauses[i];
        size_t open = 0;
        int satisfied = 0, candidate = 0;
        double score = -1.0;
        for (size_t j = 0; j < clause->count; ++j) {
            int literal = clause->literals[j], value = literal_value(solver, literal);
            if (value > 0) { satisfied = 1; break; }
            if (value == 0) {
                int var = variable(literal);
                double weight = solver->positive[var] + solver->negative[var];
                ++open;
                if (weight > score) { score = weight; candidate = var; }
            }
        }
        if (!satisfied && open && (open < shortest || (open == shortest && score > best_score))) {
            shortest = open;
            best_score = score;
            choice = candidate;
        }
    }
    if (choice && solver->negative[choice] > solver->positive[choice]) choice = -choice;
    return choice;
}

static void rollback(Solver *solver, size_t checkpoint) {
    while (solver->trail_count > checkpoint) solver->values[variable(solver->trail[--solver->trail_count])] = 0;
    solver->queue_head = checkpoint;
}

static void solver_free(Solver *solver) {
    if (solver->watches) {
        for (size_t i = 0; i < 2 * (size_t)solver->formula->variable_count; ++i) free(solver->watches[i].items);
    }
    free(solver->watches);
    free(solver->clauses);
    free(solver->trail);
    free(solver->decisions);
    free(solver->positive);
    free(solver->negative);
    free(solver->values);
}

SatStatus sat_solve(const SatFormula *formula, SatResult *result) {
    Solver solver = {0};
    SatStatus status = SAT_ERROR;
    clock_t start = clock();
    sat_result_free(result);
    if (formula->variable_count < 0 || formula->variable_count == INT_MAX) return SAT_ERROR;
    size_t n = (size_t)formula->variable_count;
    solver.formula = formula;
    solver.values = calloc(n + 1, sizeof(*solver.values));
    solver.trail = calloc(n + 1, sizeof(*solver.trail));
    solver.decisions = calloc(n + 1, sizeof(*solver.decisions));
    solver.positive = calloc(n + 1, sizeof(*solver.positive));
    solver.negative = calloc(n + 1, sizeof(*solver.negative));
    solver.clauses = calloc(formula->clause_count ? formula->clause_count : 1, sizeof(*solver.clauses));
    solver.watches = calloc(n ? 2 * n : 1, sizeof(*solver.watches));
    if (!solver.values || !solver.trail || !solver.decisions || !solver.positive || !solver.negative || !solver.clauses || !solver.watches) goto done;
    for (size_t i = 0; i < formula->clause_count; ++i) {
        const SatClause *clause = &formula->clauses[i];
        if (clause->count == 0) { status = SAT_UNSAT; goto done; }
        solver.clauses[i].positions[0] = 0;
        solver.clauses[i].positions[1] = clause->count > 1 ? 1 : 0;
        if (!watch_push(&solver.watches[literal_index(clause->literals[0])], i)) goto done;
        if (clause->count > 1 && !watch_push(&solver.watches[literal_index(clause->literals[1])], i)) goto done;
        if (clause->count == 1 && !assign_literal(&solver, clause->literals[0])) { status = SAT_UNSAT; goto done; }
        double weight = 1.0;
        for (size_t j = 0; j < clause->count && j < 1024; ++j) weight *= 0.5;
        for (size_t j = 0; j < clause->count; ++j) {
            int literal = clause->literals[j];
            if (literal > 0) solver.positive[variable(literal)] += weight;
            else solver.negative[variable(literal)] += weight;
        }
    }
    for (;;) {
        int propagated = propagate(&solver);
        if (propagated < 0) goto done;
        if (propagated == 0) {
            int retried = 0;
            ++solver.stats.conflicts;
            while (solver.decision_count) {
                Decision *decision = &solver.decisions[solver.decision_count - 1];
                rollback(&solver, decision->checkpoint);
                if (!decision->flipped) {
                    decision->flipped = 1;
                    (void)assign_literal(&solver, -decision->literal);
                    retried = 1;
                    break;
                }
                --solver.decision_count;
            }
            if (!retried) { status = SAT_UNSAT; break; }
        } else {
            int choice = choose_literal(&solver);
            if (!choice) {
                for (size_t i = 1; i <= n; ++i) if (!solver.values[i]) solver.values[i] = -1;
                status = sat_formula_is_satisfied(formula, solver.values) ? SAT_SAT : SAT_ERROR;
                break;
            }
            solver.decisions[solver.decision_count++] = (Decision){choice, 0, solver.trail_count};
            ++solver.stats.decisions;
            (void)assign_literal(&solver, choice);
        }
    }
done:
    result->status = status;
    result->variable_count = formula->variable_count;
    result->stats = solver.stats;
    result->stats.milliseconds = 1000.0 * (double)(clock() - start) / CLOCKS_PER_SEC;
    if (status == SAT_SAT) { result->values = solver.values; solver.values = NULL; }
    solver_free(&solver);
    return status;
}
