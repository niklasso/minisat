/************************************************************************************[SimpSolver.h]
Copyright (c) 2006, Niklas Een, Niklas Sorensson
Copyright (c) 2007, Niklas Sorensson

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

#ifndef Minisat_SimpSolver_h
#define Minisat_SimpSolver_h

#include <cstdio>

#include "mtl/Queue.h"
#include "core/Solver.h"


namespace Minisat {

//=================================================================================================


class SimpSolver : public Solver {
 public:
    // Constructor/Destructor:
    //
    SimpSolver();
    ~SimpSolver();

    // Problem specification:
    //
    Var     newVar    (bool polarity = true, bool dvar = true);
    bool    addClause (const vec<Lit>& ps);
    bool    addClause_(      vec<Lit>& ps);
    bool    substitute(Var v, Lit x);  // Replace all occurences of v with x (may cause a contradiction).

    // Variable mode:
    // 
    void    setFrozen (Var v, bool b); // If a variable is frozen it will not be eliminated.
    bool    isEliminated(Var v) const;
    int     nFreeVars () const { return order_heap.size(); }

    // Solving:
    //
    bool    solve     (const vec<Lit>& assumps, bool do_simp = true, bool turn_off_simp = false);
    bool    solve     (bool do_simp = true, bool turn_off_simp = false);
    bool    eliminate (bool turn_off_elim = false);  // Perform variable elimination based simplification. 

    // Generate a (possibly simplified) DIMACS file:
    //
    void    toDimacs  (const char* file);

    // Mode of operation:
    //
    int     grow;             // Allow a variable elimination step to grow by a number of clauses (default to zero).
    int     clause_lim;       // Variables are not eliminated if it produces a resolvent with a length above this limit.
                              // -1 means no limit.
    int     subsumption_lim;  // Do not check if subsumption against a clause larger than this. -1 means no limit.

    bool    use_asymm;        // Shrink clauses by asymmetric branching.
    bool    use_rcheck;       // Check if a clause is already implied. Prett costly, and subsumes subsumptions :)
    bool    use_elim;         // Perform variable elimination.

    // Statistics:
    //
    int     merges;
    int     asymm_lits;

 protected:

    // Helper structures:
    //
    struct ElimLt {
        const vec<int>& n_occ;
        ElimLt(const vec<int>& no) : n_occ(no) {}
        int  cost      (Var x)        const { return n_occ[toInt(mkLit(x))] * n_occ[toInt(~mkLit(x))]; }
        bool operator()(Var x, Var y) const { return cost(x) < cost(y); }
    };


    // Solver state:
    //
    int                 elimorder;
    bool                use_simplification;
    vec<uint32_t>       elimclauses;
    vec<char>           touched;
    vec<vec<Clause*> >  occurs;
    vec<int>            n_occ;
    Heap<ElimLt>        elim_heap;
    Queue<Clause*>      subsumption_queue;
    vec<char>           frozen;
    vec<char>           eliminated;

    int                 bwdsub_assigns;

    // Temporaries:
    //
    Clause*             bwdsub_tmpunit;

    // Main internal methods:
    //
    bool          asymm                    (Var v, Clause& c);
    bool          asymmVar                 (Var v);
    void          updateElimHeap           (Var v);
    void          cleanOcc                 (Var v);
    vec<Clause*>& getOccurs                (Var x);
    void          gatherTouchedClauses     ();
    bool          merge                    (const Clause& _ps, const Clause& _qs, Var v, vec<Lit>& out_clause);
    bool          merge                    (const Clause& _ps, const Clause& _qs, Var v, int& size);
    bool          backwardSubsumptionCheck (bool verbose = false);
    bool          eliminateVar             (Var v);
    void          extendModel              ();

    void          removeClause             (Clause& c);
    bool          strengthenClause         (Clause& c, Lit l);
    void          cleanUpClauses           ();
    bool          implied                  (const vec<Lit>& c);
    //void          toDimacs                 (FILE* f, Clause& c);
    void          toDimacs                 (FILE* f, Clause& c, vec<Var>& map, Var& max);
};


//=================================================================================================
// Implementation of inline methods:


inline bool SimpSolver::isEliminated (Var v) const { return eliminated[v]; }
inline void SimpSolver::updateElimHeap(Var v) {
    if (!frozen[v] && !isEliminated(v) && value(v) == l_Undef)
        elim_heap.update(v); }

inline void SimpSolver::cleanOcc(Var v) {
    assert(use_simplification);
    Clause **begin = (Clause**)occurs[v];
    Clause **end = begin + occurs[v].size();
    Clause **i, **j;
    for (i = begin, j = end; i < j; i++)
        if ((*i)->mark() == 1){
            *i = *(--j);
            i--;
        }
    //occurs[v].shrink_(end - j);  // This seems slower. Why?!
    occurs[v].shrink(end - j);
}

inline vec<Clause*>& SimpSolver::getOccurs(Var x) {
    cleanOcc(x); return occurs[x]; }

inline bool SimpSolver::addClause    (const vec<Lit>& ps) { ps.copyTo(add_tmp); return addClause_(add_tmp); }
inline void SimpSolver::setFrozen    (Var v, bool b) { frozen[v] = (char)b; if (b) { updateElimHeap(v); } }
inline bool SimpSolver::solve        (bool do_simp, bool turn_off_simp) { vec<Lit> tmp; return solve(tmp, do_simp, turn_off_simp); }

//=================================================================================================

};

#endif
