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

#include <ctime>
#include <cstring>
#include <stdint.h>
#include <errno.h>

#include <signal.h>
#include <zlib.h>

#include "SimpSolver.h"

/*************************************************************************************/
#ifdef _MSC_VER
#include <ctime>

static inline double cpuTime(void) {
    return (double)clock() / CLOCKS_PER_SEC; }
#else

#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>

static inline double cpuTime(void) {
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    return (double)ru.ru_utime.tv_sec + (double)ru.ru_utime.tv_usec / 1000000; }
#endif


#if defined(__linux__)
static inline int memReadStat(int field)
{
    char    name[256];
    pid_t pid = getpid();
    sprintf(name, "/proc/%d/statm", pid);
    FILE*   in = fopen(name, "rb");
    if (in == NULL) return 0;
    int     value;
    for (; field >= 0; field--)
        fscanf(in, "%d", &value);
    fclose(in);
    return value;
}
static inline uint64_t memUsed() { return (uint64_t)memReadStat(0) * (uint64_t)getpagesize(); }


#elif defined(__FreeBSD__)
static inline uint64_t memUsed(void) {
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    return ru.ru_maxrss*1024; }


#elif defined(__APPLE__)
#include <malloc/malloc.h>

static inline uint64_t memUsed(void) {
    malloc_statistics_t t;
    malloc_zone_statistics(NULL, &t);
    return t.max_size_in_use; }


#else
static inline uint64_t memUsed() { return 0; }
#endif

#if defined(__linux__)
#include <fpu_control.h>
#endif


//=================================================================================================
// DIMACS Parser:

#define CHUNK_LIMIT 1048576

class StreamBuffer {
    gzFile  in;
    char    buf[CHUNK_LIMIT];
    int     pos;
    int     size;

    void assureLookahead() {
        if (pos >= size) {
            pos  = 0;
            size = gzread(in, buf, sizeof(buf)); } }

public:
    StreamBuffer(gzFile i) : in(i), pos(0), size(0) {
        assureLookahead(); }

    int  operator *  () { return (pos >= size) ? EOF : buf[pos]; }
    void operator ++ () { pos++; assureLookahead(); }
};

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

template<class B>
static void skipWhitespace(B& in) {
    while ((*in >= 9 && *in <= 13) || *in == 32)
        ++in; }

template<class B>
static void skipLine(B& in) {
    for (;;){
        if (*in == EOF || *in == '\0') return;
        if (*in == '\n') { ++in; return; }
        ++in; } }

template<class B>
static int parseInt(B& in) {
    int     val = 0;
    bool    neg = false;
    skipWhitespace(in);
    if      (*in == '-') neg = true, ++in;
    else if (*in == '+') ++in;
    if (*in < '0' || *in > '9') reportf("PARSE ERROR! Unexpected char: %c\n", *in), exit(3);
    while (*in >= '0' && *in <= '9')
        val = val*10 + (*in - '0'),
        ++in;
    return neg ? -val : val; }

template<class B>
static void readClause(B& in, SimpSolver& S, vec<Lit>& lits) {
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
static bool match(B& in, const char* str) {
    for (; *str != 0; ++str, ++in)
        if (*str != *in)
            return false;
    return true;
}


template<class B>
static void parse_DIMACS_main(B& in, SimpSolver& S) {
    vec<Lit> lits;
    int vars    = 0;
    int clauses = 0;
    int cnt     = 0;
    for (;;){
        if (S.verbosity == 2 && S.use_rcheck && S.nClauses() % 100 == 0)
            printf("Redundancy check: %10d/%10d (%10d)\r", S.nClauses(), clauses, cnt - S.nClauses());

        skipWhitespace(in);
        if (*in == EOF) break;
        else if (*in == 'p'){
            if (match(in, "p cnf")){
                vars    = parseInt(in);
                clauses = parseInt(in);
                reportf("|  Number of variables:  %12d                                         |\n", vars);
                reportf("|  Number of clauses:    %12d                                         |\n", clauses);
                
                // SATRACE'06 hack
                if (clauses > 4000000)
                    S.eliminate(true);
            }else{
                reportf("PARSE ERROR! Unexpected char: %c\n", *in), exit(3);
            }
        } else if (*in == 'c' || *in == 'p')
            skipLine(in);
        else{
            cnt++;
            readClause(in, S, lits);
            S.addClause(lits); }
    }
}

// Inserts problem into solver.
//
static void parse_DIMACS(gzFile input_stream, SimpSolver& S) {
    StreamBuffer in(input_stream);
    parse_DIMACS_main(in, S); }


//=================================================================================================


void printStats(Solver& S)
{
    double   cpu_time = cpuTime();
    uint64_t mem_used = memUsed();
    reportf("restarts              : %lld\n", S.starts);
    reportf("conflicts             : %-12lld   (%.0f /sec)\n", S.conflicts   , S.conflicts   /cpu_time);
    reportf("decisions             : %-12lld   (%4.2f %% random) (%.0f /sec)\n", S.decisions, (float)S.rnd_decisions*100 / (float)S.decisions, S.decisions   /cpu_time);
    reportf("propagations          : %-12lld   (%.0f /sec)\n", S.propagations, S.propagations/cpu_time);
    reportf("conflict literals     : %-12lld   (%4.2f %% deleted)\n", S.tot_literals, (S.max_literals - S.tot_literals)*100 / (double)S.max_literals);
    if (mem_used != 0) reportf("Memory used           : %.2f MB\n", mem_used / 1048576.0);
    reportf("CPU time              : %g s\n", cpu_time);
}

SimpSolver* solver;
static void SIGINT_handler(int signum) {
    reportf("\n"); reportf("*** INTERRUPTED ***\n");
    printStats(*solver);
    reportf("\n"); reportf("*** INTERRUPTED ***\n");
    exit(1); }


//=================================================================================================
// Main:

void printUsage(char** argv, SimpSolver& S)
{
    reportf("USAGE: %s [options] <input-file> <result-output-file>\n\n  where input may be either in plain or gzipped DIMACS.\n\n", argv[0]);
    reportf("OPTIONS:\n\n");
    reportf("  -pre,    -no-pre                     (default: on)\n");
    reportf("  -elim,   -no-elim                    (default: %s)\n", S.use_elim           ? "on" : "off");
    reportf("  -asymm,  -no-asymm                   (default: %s)\n", S.use_asymm          ? "on" : "off");
    reportf("  -rcheck, -no-rcheck                  (default: %s)\n", S.use_rcheck         ? "on" : "off");
    reportf("\n");
    reportf("  -grow          = <integer> [ >= 0  ] (default: %d)\n", S.grow);
    reportf("  -lim           = <integer> [ >= -1 ] (default: %d)\n", S.clause_lim);
    reportf("  -decay         = <double>  [ 0 - 1 ] (default: %g)\n", S.var_decay);
    reportf("  -rnd-freq      = <double>  [ 0 - 1 ] (default: %g)\n", S.random_var_freq);
    reportf("\n");
    reportf("  -dimacs        = <output-file>.\n");
    reportf("  -verbosity     = {0,1,2}             (default: %d)\n", S.verbosity);
    reportf("\n");
}

const char* hasPrefix(const char* str, const char* prefix)
{
    int len = strlen(prefix);
    if (strncmp(str, prefix, len) == 0)
        return str + len;
    else
        return NULL;
}


int main(int argc, char** argv)
{
    reportf("This is MiniSat 2.0 beta\n");
#if defined(__linux__)
    fpu_control_t oldcw, newcw;
    _FPU_GETCW(oldcw); newcw = (oldcw & ~_FPU_EXTENDED) | _FPU_DOUBLE; _FPU_SETCW(newcw);
    reportf("WARNING: for repeatability, setting FPU to use double precision\n");
#endif
    bool           pre    = true;
    const char*    dimacs = NULL;
    SimpSolver     S;
    S.verbosity = 1;

    // Check for help flag:
    for (int i = 0; i < argc; i++)
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "-help") == 0){
            printUsage(argv, S);
            exit(0); }

    // This just grew and grew, and I didn't have time to do sensible argument parsing yet :)
    //
    int         i, j;
    const char* value;
    for (i = j = 0; i < argc; i++){
        if ((value = hasPrefix(argv[i], "-rnd-freq="))){
            double rnd;
            if (sscanf(value, "%lf", &rnd) <= 0 || rnd < 0 || rnd > 1){
                reportf("ERROR! illegal rnd-freq constant %s\n", value);
                exit(0); }
            S.random_var_freq = rnd;

        }else if ((value = hasPrefix(argv[i], "-decay="))){
            double decay;
            if (sscanf(value, "%lf", &decay) <= 0 || decay <= 0 || decay > 1){
                reportf("ERROR! illegal decay constant %s\n", value);
                exit(0); }
            S.var_decay = 1 / decay;

        }else if ((value = hasPrefix(argv[i], "-verbosity="))){
            int verbosity = (int)strtol(value, NULL, 10);
            if (verbosity == 0 && errno == EINVAL){
                reportf("ERROR! illegal verbosity level %s\n", value);
                exit(0); }
            S.verbosity = verbosity;

        // Boolean flags:
        //
        }else if (strcmp(argv[i], "-pre") == 0){
            pre = true;
        }else if (strcmp(argv[i], "-no-pre") == 0){
            pre = false;
        }else if (strcmp(argv[i], "-asymm") == 0){
            S.use_asymm = true;
        }else if (strcmp(argv[i], "-no-asymm") == 0){
            S.use_asymm = false;
        }else if (strcmp(argv[i], "-rcheck") == 0){
            S.use_rcheck = true;
        }else if (strcmp(argv[i], "-no-rcheck") == 0){
            S.use_rcheck = false;
        }else if (strcmp(argv[i], "-elim") == 0){
            S.use_elim = true;
        }else if (strcmp(argv[i], "-no-elim") == 0){
            S.use_elim = false;
        }else if ((value = hasPrefix(argv[i], "-grow="))){
            int grow = (int)strtol(value, NULL, 10);
            if (grow < 0){
                reportf("ERROR! illegal grow constant %s\n", &argv[i][6]);
                exit(0); }
            S.grow = grow;
        }else if ((value = hasPrefix(argv[i], "-lim="))){
            int lim = (int)strtol(value, NULL, 10);
            if (lim < 3){
                reportf("ERROR! illegal clause limit constant %s\n", &argv[i][5]);
                exit(0); }
            S.clause_lim = lim;
        }else if ((value = hasPrefix(argv[i], "-dimacs="))){
            dimacs = value;
        }else if (strncmp(argv[i], "-", 1) == 0){
            reportf("ERROR! unknown flag %s\nUse -help for more information.\n", argv[i]);
            exit(0);
        }else
            argv[j++] = argv[i];
    }
    argc = j;

    double initial_time = cpuTime();

    if (!pre) S.eliminate(true);

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

    S.eliminate(true);
    double simplified_time = cpuTime();
    reportf("|  Simplification time:  %12.2f s                                       |\n", simplified_time - parsed_time);
    reportf("|                                                                             |\n");

    if (!S.okay()){
        if (res != NULL) fprintf(res, "UNSAT\n"), fclose(res);
        reportf("===============================================================================\n");
        reportf("Solved by simplification\n");
        printStats(S);
        reportf("\n");
        printf("UNSATISFIABLE\n");
        exit(20);
    }

    if (dimacs){
        reportf("==============================[ Writing DIMACS ]===============================\n");
        S.toDimacs(dimacs);
        printStats(S);
        exit(0);
    }else{
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

}
