/*
 * verify_model.c
 *
 * Friedrich GrÃ¤ter, December 2008
 *
 */
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int32_t literal_t;
typedef uint32_t variable_t;

typedef enum { POS = 1, NEG = 2, UNDEF = 0 } polarity_t;

static inline literal_t variable_to_literal(variable_t var, polarity_t pol)
{
    return (literal_t)var * ((pol == POS) ? (1) : (-1));
}
static inline literal_t invert_literal(literal_t lit) { return -lit; }
static inline variable_t literal_to_variable(literal_t lit) { return (lit > 0) ? (lit) : (-lit); }
static inline polarity_t literal_get_polarity(literal_t lit)
{
    return (lit > 0) ? (POS) : ((lit < 0) ? (NEG) : (UNDEF));
}

/*
 * libpdex_search_for(file, symbol)
 *
 * Searches for a line beginning with "symbol" in "file".
 * Whitespace and comment lines will be ignored. The file
 * will be read from its beginning. The function lefts the
 * file pointer after the occurence of "symbol".
 *
 * Return value:
 *      1   Symbol found
 *      0   Symbol not found, syntax or I/O error
 */
static int libpdex_search_for(FILE *file, int symbol)
{
    rewind(file);

    while (!feof(file)) {
        int c;

        c = fgetc(file);

        switch (c) {
        case EOF:
            return 0;

        case ' ':
        case '\n':
        case '\t':
            break;

        case 'c': {
            char l;

            do {
                l = fgetc(file);

                if (l == '\n') {
                    break;
                }
            } while (l != EOF);

            break;
        }

        default:
            if (c == symbol) {
                return 1;
            }
        }
    }

    return 0;
}

/*
 * libpdex_get_stat_state(file)
 *
 * Finds out whether the given model file provides
 * a satisfying or unsatisfying model. Comments and
 * whitespace will be ignored.
 *
 * Return value:
 *      1   SAT
 *      0   UNSAT
 *      -1  ERROR
 *
 */
static int libpdex_get_sat_state(FILE *file)
{
    if (libpdex_search_for(file, 's')) {
        char buffer[100];
        int is_sat;
        size_t buf_len;

        fgets(buffer, 100, file);

        // Remove whitespaces at the end of the line
        buf_len = strlen(buffer);

        while (buf_len--) {
            if ((buffer[buf_len] == ' ') || (buffer[buf_len] == '\n')) {
                buffer[buf_len] = 0;
            } else {
                break;
            }
        }

        // Compare
        is_sat = (strcmp(buffer, " SATISFIABLE") == 0);

        /* Invalid solver output */
        if ((!is_sat) && (strcmp(buffer, " UNSATISFIABLE") != 0)) {
            return -1;
        }

        return is_sat;
    }

    return -1;
}

/*
 * libpdex_satisfies_problem(model, model_ctr, problem, clause_ctr)
 *
 * Tests whether the given "model" (an map from variable_t => polarity_t) satisfies
 * the given clauses (an array of zero-terminated arrays of literal_t).
 * "model_ctr" contains the count of literals in the model and "clause_ctr"
 * the count of clauses of the problem.
 *
 * Return value:
 *      1       Correct
 *      0       Invalid
 *
 */
static int libpdex_satisfies_problem(polarity_t *model, size_t model_ctr, literal_t **problem, size_t clause_ctr)
{
    size_t clause_id;

    int ret = 1;
    // Propagate literal
    for (clause_id = 0; clause_id < clause_ctr; clause_id++) {
        literal_t *clause = problem[clause_id];
        bool satisfied = false;

        while (*clause != 0) {
            literal_t lit = *clause;
            variable_t var = literal_to_variable(lit);

            // Model too small
            if (var >= model_ctr) {
                printf("found variable that is not present in model: %d\n", var);
                return 0;
            }

            // Clause is satisfied
            if (model[var] == literal_get_polarity(lit)) {
                satisfied = true;
                break;
            }

            clause++;
        }

        if (!satisfied) {
            literal_t *clause = problem[clause_id];
            printf("unsat clause: ");
            while (*clause != 0) {
                printf("%d ", *clause);
                clause++;
            }
            printf("\n");
            ret = 0;
        }
    }

    // If no clause left, we have a satisfying model
    return ret;
}

/*
 * libpdex_read_literal(stream)
 *
 * Reads a literal from a given DIMACS stream.
 *
 */
literal_t libpdex_read_literal(FILE *stream)
{
    polarity_t pol = POS;
    variable_t lit = 0;
    bool nothing_read = true;

    while (1) {
        int c = fgetc(stream);

        if (feof(stream)) {
            return lit;
        }

        /* Comment: just go to the next line and return the literal */
        if ((c == 'c') || (c == 'C')) {
            while (1) {
                c = fgetc(stream);

                if ((c == '\n') || feof(stream)) {
                    return lit;
                }
            }
        }

        /* Ignore any invalid charracter before a literal begins */
        if (((c < '0') || (c > '9')) && (c != '-') && (nothing_read)) {
            continue;
        } else {
            nothing_read = false;
        }

        /* End of literal */
        if ((c == ' ') || (c == '\n') || (c == EOF)) {
            break;
        }

        /* Negation */
        if (c == '-') {
            pol = NEG;
            continue;
        }

        /* Ignore invalid charracters */
        if ((c < '0') || (c > '9')) {
            printf("READ ERROR");
            exit(1);
        }

        /* Calculate literal name */
        lit *= 10;
        lit += (c - '0');
    }

    return variable_to_literal(lit, pol);
}

/*
 * libpdex_read_problem(file, &clause_ctr)
 *
 * Reads a problem file and creates an array of clauses. Each
 * clause is represented by a zero-terminated array of literals.
 * "clause_ctr" returns the count of clauses in the array.
 *
 */
literal_t **libpdex_read_problem(FILE *file, size_t *clause_ctr)
{
    literal_t **clauses = NULL;
    literal_t lit;
    bool pline;
    char c;
    size_t clause_count = 0;

    // Init
    pline = false;

    // Read lines
    while (!feof(file)) {
        // Check P-Line (we will ignore its informations)
        if (fscanf(file, "p%c", &c)) {
            unsigned var_ctr, cls_ctr;

            fscanf(file, "cnf %u %u\n", (unsigned *)&var_ctr, (unsigned *)&cls_ctr);
            pline = true;

            continue;
        }

        // Read Clause
        literal_t *lits = NULL;
        size_t lits_count = 0;

        while (!feof(file)) {
            lit = libpdex_read_literal(file);

            if ((feof(file) && (lits_count == 0)) || ((lit == 0) && (lits_count == 0))) {
                break;
            }

            lits_count++;
            lits = (literal_t *)realloc(lits, lits_count * sizeof(literal_t));
            lits[lits_count - 1] = lit;

            if (lit == 0) {
                break;
            }
        }

        if (lits_count) {
            clause_count++;
            clauses = (literal_t **)realloc(clauses, clause_count * sizeof(literal_t *));
            clauses[clause_count - 1] = lits;
        }
    }
    *clause_ctr = clause_count;

    return clauses;
}

/*
 * libpdex_satisfies_problem_file(problem, model)
 *
 * Tests whether the given output "model" satisfies "problem".
 *
 * Return value:
 *      1       Correct
 *      0       Invalid
 *      -1      I/O Error
 *
 */
static int libpdex_satisfies_problem_file(FILE *problem, FILE *model)
{
    literal_t *parsed_model = NULL;
    polarity_t *model_map = NULL;
    literal_t **parsed_problem = NULL;
    size_t model_ctr = 0;
    size_t clause_ctr = 0;

    /* Find the model data */
    if (!libpdex_search_for(model, 'v')) {
        return -1;
    }

    /* Get the model */
    while (!feof(model)) {
        literal_t literal;

        if (fscanf(model, "%i", &literal) != 1) {
            break;
        }

        model_ctr++;
        parsed_model = (literal_t *)realloc(parsed_model, model_ctr * sizeof(literal_t));

        parsed_model[model_ctr - 1] = literal;
    }

    /* Build the model map */
    model_map = (polarity_t *)malloc(model_ctr * sizeof(polarity_t));

    size_t map_ctr;
    for (map_ctr = 0; map_ctr < model_ctr; map_ctr++) {
        literal_t lit = parsed_model[map_ctr];
        variable_t var = literal_to_variable(lit);

        model_map[var] = literal_get_polarity(lit);
    }

    /* Get all clauses from the problem  */
    parsed_problem = libpdex_read_problem(problem, &clause_ctr);

    return libpdex_satisfies_problem(model_map, model_ctr, parsed_problem, clause_ctr);
}

/*
 * libpdex_verify_model
 *
 * Compares the stdout output of the solver with the
 * given model. The verifying is specified to the DIMACS
 * format.
 *
 * Return values:
 *      -1          ERROR
 *       0          Model correct
 *       1          Incorrect model
 *
 */
int libpdex_verify_model(FILE *problem_file, FILE *solver_stdout, bool model_is_sat)
{
    int solver_is_sat;

    /* Test whether the model is SAT or not */
    solver_is_sat = libpdex_get_sat_state(solver_stdout);

    if (model_is_sat == -1) {
        fprintf(stderr, "Failed to get model state.\n");

        return -1;
    }

    if (solver_is_sat == -1) {
        fprintf(stderr, "Invalid solver output.\n");

        return 0;
    }

    /* Model and solver telling different things */
    if (solver_is_sat != model_is_sat)

    {
        return 0;
    }

    /* If UNSAT, just compare states */
    if (solver_is_sat == 0) {
        return (model_is_sat == solver_is_sat) ? (1) : (0);
    }

    /* If SAT, verify given models */
    if (solver_is_sat == 1) {
        int model_validity;

        model_validity = libpdex_satisfies_problem_file(problem_file, solver_stdout);

        if (model_validity == -1) {
            fprintf(stderr, "Can't compare models.\n");

            return -1;
        }

        return model_validity;
    }
}

/*
 * main
 *
 */
void print_usage(const char *name)
{
    printf("USAGE:\n");
    printf("\t%s SAT <solver-output> <problem.cnf>\n", name);
    printf("\t%s UNSAT <solver-output> <problem.cnf>\n\n", name);

    exit(0);
}

int main(int argc, char *argv[])
{
    FILE *problem, *solver_out;
    bool model_is_sat = false;

    // Read arguments and test their validity
    if (argc < 3) {
        print_usage(argv[0]);
    }

    if (strcmp(argv[1], "SAT") == 0) {
        model_is_sat = true;
    } else if (strcmp(argv[1], "UNSAT")) {
        print_usage(argv[0]);
    }

    if ((model_is_sat) && (argc < 4)) {
        print_usage(argv[0]);
    }

    // Open input streams
    solver_out = fopen(argv[2], "r");
    if (solver_out == NULL) {
        perror("Can't open solver output.");
        exit(-1);
    }

    if (model_is_sat) {
        problem = fopen(argv[3], "r");

        if (problem == NULL) {
            perror("Can't open problem output.");
            exit(-1);
        }

    } else {
        problem = NULL;
    }

    // Verify model
    switch (libpdex_verify_model(problem, solver_out, model_is_sat)) {
    case -1:
        perror("I/O or parser error. System I/O state");
        return -1;

    case 0:
        printf("Invalid model.\n");
        return 0;

    case 1:
        printf("Model valid.\n");
        return 1;
    }
}
