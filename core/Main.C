/******************************************************************************************[Main.C]
Copyright (c) 2003-2006, Niklas Een, Niklas Sorensson
Copyright (c) 2007,      Niklas Sorensson

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#include <errno.h>

#include <signal.h>
#include <zlib.h>

#include "System.h"
#include "ParseUtils.h"
#include "Options.h"

#include "Solver.h"

//=================================================================================================
// DIMACS Parser:

template<class B>
static void readClause(B& in, Solver& S, vec<Lit>& lits) {
    int     parsed_lit, var;
    lits.clear();
    for (;;){
        parsed_lit = parseInt(in);
        if (parsed_lit == 0) break;
        var = abs(parsed_lit)-1;
        while (var >= S.nVars()) S.newVar();
        lits.push( (parsed_lit > 0) ? mkLit(var) : ~mkLit(var) );
    }
}

template<class B>
static void parse_DIMACS_main(B& in, Solver& S) {
    vec<Lit> lits;
    for (;;){
        skipWhitespace(in);
        if (*in == EOF)
            break;
        else if (*in == 'p'){
            if (eagerMatch(in, "p cnf")){
                int vars    = parseInt(in);
                int clauses = parseInt(in);
                reportf("|  Number of variables:  %12d                                         |\n", vars);
                reportf("|  Number of clauses:    %12d                                         |\n", clauses);
            }else{
                reportf("PARSE ERROR! Unexpected char: %c\n", *in), exit(3);
            }
        } else if (*in == 'c' || *in == 'p')
            skipLine(in);
        else
            readClause(in, S, lits),
            S.addClause(lits);
    }
}

// Inserts problem into solver.
//
static void parse_DIMACS(gzFile input_stream, Solver& S) {
    StreamBuffer in(input_stream);
    parse_DIMACS_main(in, S); }


//=================================================================================================


void printStats(Solver& solver)
{
    double   cpu_time = cpuTime();
    uint64_t mem_used = memUsed();
    reportf("restarts              : %"PRIu64"\n", solver.starts);
    reportf("conflicts             : %-12"PRIu64"   (%.0f /sec)\n", solver.conflicts   , solver.conflicts   /cpu_time);
    reportf("decisions             : %-12"PRIu64"   (%4.2f %% random) (%.0f /sec)\n", solver.decisions, (float)solver.rnd_decisions*100 / (float)solver.decisions, solver.decisions   /cpu_time);
    reportf("propagations          : %-12"PRIu64"   (%.0f /sec)\n", solver.propagations, solver.propagations/cpu_time);
    reportf("conflict literals     : %-12"PRIu64"   (%4.2f %% deleted)\n", solver.tot_literals, (solver.max_literals - solver.tot_literals)*100 / (double)solver.max_literals);
    if (mem_used != 0) reportf("Memory used           : %.2f MB\n", mem_used / 1048576.0);
    reportf("CPU time              : %g s\n", cpu_time);
}

Solver* solver;
static void SIGINT_handler(int signum) {
    reportf("\n"); reportf("*** INTERRUPTED ***\n");
    printStats(*solver);
    reportf("\n"); reportf("*** INTERRUPTED ***\n");
    exit(1); }


//=================================================================================================
// Main:

void printUsage(char** argv, Solver& S)
{
    reportf("USAGE: %s [options] <input-file> <result-output-file>\n\n  where input may be either in plain or gzipped DIMACS.\n\n", argv[0]);
    reportf("OPTIONS:\n\n");
    reportf("  -decay         = <double>  [ 0 - 1 ] (default: %g)\n", S.var_decay);
    reportf("  -rnd-freq      = <double>  [ 0 - 1 ] (default: %g)\n", S.random_var_freq);
    reportf("  -seed          = <double>  [ >0    ] (default: %g)\n", S.random_seed);
    reportf("  -verb          = {0,1,2}             (default: %d)\n", S.verbosity);
    reportf("\n");
}


int main(int argc, char** argv)
{
    Solver      S;
    S.verbosity = 1;


    int         i, j;
    const char* value;
    for (i = j = 0; i < argc; i++){
        value = argv[i];

        if (match(value, "-rnd-freq=")){
            double rnd;
            if (sscanf(value, "%lf", &rnd) <= 0 || rnd < 0 || rnd > 1){
                reportf("ERROR! illegal rnd-freq constant %s\n", value);
                exit(0); }
            S.random_var_freq = rnd;

        }else if (match(value, "-decay=")){
            double decay;
            if (sscanf(value, "%lf", &decay) <= 0 || decay <= 0 || decay > 1){
                reportf("ERROR! illegal decay constant %s\n", value);
                exit(0); }
            S.var_decay = 1 / decay;

        }else if (match(value, "-seed=")){
            double seed;
            if (sscanf(value, "%lf", &seed) <= 0 || seed <= 0){
                reportf("ERROR! illegal random seed constant %s\n", value);
                exit(0); }
            S.random_seed = seed;

        }else if (match(value, "-verb=")){
            int verbosity = (int)strtol(value, NULL, 10);
            if (verbosity == 0 && errno == EINVAL){
                reportf("ERROR! illegal verbosity level %s\n", value);
                exit(0); }
            S.verbosity = verbosity;

        }else if (match(value, "-h") || match(value, "-help") || match(value, "--help")){
            printUsage(argv, S);
            exit(0);

        }else if (match(value, "-")){
            reportf("ERROR! unknown flag %s\n", value);
            exit(0);

        }else
            argv[j++] = argv[i];
    }
    argc = j;


    reportf("This is MiniSat 2.0 beta\n");
#if defined(__linux__)
    fpu_control_t oldcw, newcw;
    _FPU_GETCW(oldcw); newcw = (oldcw & ~_FPU_EXTENDED) | _FPU_DOUBLE; _FPU_SETCW(newcw);
    reportf("WARNING: for repeatability, setting FPU to use double precision\n");
#endif
    double initial_time = cpuTime();

    solver = &S;
    signal(SIGINT,SIGINT_handler);
    signal(SIGHUP,SIGINT_handler);

    if (argc == 1)
        reportf("Reading from standard input... Use '-h' or '--help' for help.\n");

    gzFile in = (argc == 1) ? gzdopen(0, "rb") : gzopen(argv[1], "rb");
    if (in == NULL)
        reportf("ERROR! Could not open file: %s\n", argc == 1 ? "<stdin>" : argv[1]), exit(1);

    reportf("============================[ Problem Statistics ]=============================\n");
    reportf("|                                                                             |\n");

    parse_DIMACS(in, S);
    gzclose(in);
    FILE* res = (argc >= 3) ? fopen(argv[2], "wb") : NULL;

    double parsed_time = cpuTime();
    reportf("|  Parse time:           %12.2f s                                       |\n", parsed_time - initial_time);
    reportf("|                                                                             |\n");

    if (!S.simplify()){
        if (res != NULL) fprintf(res, "UNSAT\n"), fclose(res);
        reportf("===============================================================================\n");
        reportf("Solved by unit propagation\n");
        printStats(S);
        reportf("\n");
        printf("UNSATISFIABLE\n");
        exit(20);
    }

    bool ret = S.solve();
    printStats(S);
    reportf("\n");
    printf(ret ? "SATISFIABLE\n" : "UNSATISFIABLE\n");
    if (res != NULL){
        if (ret){
            fprintf(res, "SAT\n");
            for (int i = 0; i < S.nVars(); i++)
                if (S.model[i] != l_Undef)
                    fprintf(res, "%s%s%d", (i==0)?"":" ", (S.model[i]==l_True)?"":"-", i+1);
            fprintf(res, " 0\n");
        }else
            fprintf(res, "UNSAT\n");
        fclose(res);
    }

#ifdef NDEBUG
    exit(ret ? 10 : 20);     // (faster than "return", which will invoke the destructor for 'Solver')
#endif
}
