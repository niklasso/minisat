/*****************************************************************************************[Main.cc]
Copyright (c) 2003-2006, Niklas Een, Niklas Sorensson
Copyright (c) 2007-2010, Niklas Sorensson

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
#include <zlib.h>

#include "minisat/utils/System.h"
#include "minisat/utils/ParseUtils.h"
#include "minisat/utils/Options.h"
#include "minisat/core/Dimacs.h"
#include "minisat/core/Solver.h"

#include <sstream>

using namespace Minisat;

//=================================================================================================


static Solver* solver;
// Terminate by notifying the solver and back out gracefully. This is mainly to have a test-case
// for this feature of the Solver as it may take longer than an immediate call to '_exit()'.
static void SIGINT_interrupt(int) { solver->interrupt(); }

// Note that '_exit()' rather than 'exit()' has to be used. The reason is that 'exit()' calls
// destructors and may cause deadlocks if a malloc/free function happens to be running (these
// functions are guarded by locks for multithreaded use).
static void SIGINT_exit(int) {
    printf("c \n"); printf("c *** INTERRUPTED ***\n");
    if (solver->verbosity > 0){
        solver->printStats();
        printf("c \n"); printf("c *** INTERRUPTED ***\n"); }
    _exit(1); }


//=================================================================================================
// Main:


int main(int argc, char** argv)
{
    try {
        setUsageHelp("USAGE: %s [options] <input-file> <result-output-file>\n\n  where input may be either in plain or gzipped DIMACS.\n");
        setX86FPUPrecision();

        // Extra options:
        //
        IntOption    verb   ("MAIN", "verb",   "Verbosity level (0=silent, 1=some, 2=more).", 1, IntRange(0, 2));
        IntOption    cpu_lim("MAIN", "cpu-lim","Limit on CPU time allowed in seconds.\n", 0, IntRange(0, INT32_MAX));
        IntOption    mem_lim("MAIN", "mem-lim","Limit on memory usage in megabytes.\n", 0, IntRange(0, INT32_MAX));
        BoolOption   strictp("MAIN", "strict", "Validate DIMACS header during parsing.", false);
        BoolOption   model  ("MAIN", "model", "Print the values for the model in case of satisfiable.", true);
        StringOption proof  ("MAIN", "proof",  "Given a filename, a DRAT proof will be written there.");
        
        parseOptions(argc, argv, true);

        Solver S;
        double initial_time = cpuTime();

        S.verbosity = verb;
        
        solver = &S;
        // Use signal handlers that forcibly quit until the solver will be able to respond to
        // interrupts:
        sigTerm(SIGINT_exit);

        // Try to set resource limits:
        if (cpu_lim != 0) limitTime(cpu_lim);
        if (mem_lim != 0) limitMemory(mem_lim);
        
        if (argc == 1)
            printf("c Reading from standard input... Use '--help' for help.\n");
        
        gzFile in = (argc == 1) ? gzdopen(0, "rb") : gzopen(argv[1], "rb");
        if (in == NULL)
            printf("c ERROR! Could not open file: %s\n", argc == 1 ? "<stdin>" : argv[1]), exit(1);
        
        if (S.verbosity > 0){
            printf("c ============================[ Problem Statistics ]=============================\n");
            printf("c |                                                                             |\n"); }
        
        if((const char *)proof)
            if(!S.openProofFile((const char *)proof)){
                printf("c ERROR! Could not open proof file: %s\n", (const char *)proof);
                exit(1);
            }

        parse_DIMACS(in, S, (bool)strictp);
        gzclose(in);
        FILE* res = (argc >= 3) ? fopen(argv[2], "wb") : NULL;
        
        if (S.verbosity > 0){
            printf("c |  Number of variables:  %12d                                         |\n", S.nVars());
            printf("c |  Number of clauses:    %12d                                         |\n", S.nClauses()); }
        
        double parsed_time = cpuTime();
        if (S.verbosity > 0){
            printf("c |  Parse time:           %12.2f s                                       |\n", parsed_time - initial_time);
            printf("c |                                                                             |\n"); }
 
        // Change to signal-handlers that will only notify the solver and allow it to terminate
        // voluntarily:
        sigTerm(SIGINT_interrupt);
       
        if (!S.simplify()){
            S.finalizeProof(true);
            if (res != NULL) fprintf(res, "UNSAT\n"), fclose(res);
            if (S.verbosity > 0){
                printf("c ===============================================================================\n");
                printf("c Solved by unit propagation\n");
                S.printStats();
                printf("c \n"); }
            printf("s UNSATISFIABLE\n");
            exit(20);
        }
        
        vec<Lit> dummy;
        lbool ret = S.solveLimited(dummy);
        if (S.verbosity > 0){
            S.printStats();
            printf("c \n"); }
        printf(ret == l_True ? "s SATISFIABLE\n" : ret == l_False ? "s UNSATISFIABLE\n" : "s UNKNOWN\n");
        S.finalizeProof(ret == l_False);
        if (ret == l_True && model){
                std::stringstream s;
                for (int i = 0; i < S.nVars(); i++)
                    s << ((S.model[i]==l_True)?i+1:-i-1) << " ";
                printf("v %s0\n", s.str().c_str());
        }
        if (res != NULL){
            if (ret == l_True){
                fprintf(res, "s SATISFIABLE\n");
                for (int i = 0; i < S.nVars(); i++)
                    if (S.model[i] != l_Undef)
                        fprintf(res, "%s%s%d", (i==0)?"":" ", (S.model[i]==l_True)?"":"-", i+1);
                fprintf(res, " 0\n");
            }else if (ret == l_False)
                fprintf(res, "s UNSATISFIABLE\n");
            else
                fprintf(res, "s UNKNOWN\n");
            fclose(res);
        }
        
#ifdef NDEBUG
        exit(ret == l_True ? 10 : ret == l_False ? 20 : 0);     // (faster than "return", which will invoke the destructor for 'Solver')
#else
        return (ret == l_True ? 10 : ret == l_False ? 20 : 0);
#endif
    } catch (OutOfMemoryException&){
        printf("s UNKNOWN\n");
        exit(0);
    }
}
