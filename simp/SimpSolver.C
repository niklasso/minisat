/************************************************************************************[SimpSolver.C]
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

#include "mtl/Sort.h"
#include "simp/SimpSolver.h"

using namespace Minisat;

//=================================================================================================
// Options:


const char* _cat = "SIMP";

static BoolOption opt_use_asymm          (_cat, "asymm",   "Shrink clauses by asymmetric branching.", false);
static BoolOption opt_use_rcheck         (_cat, "rcheck",  "Check if a clause is already implied. (costly)", false);
static BoolOption opt_use_elim           (_cat, "elim",    "Perform variable elimination.", true);
static IntOption  opt_grow               (_cat, "grow",    "Allow a variable elimination step to grow by a number of clauses.", 0);
static IntOption  opt_clause_lim         (_cat, "cl-lim",  "Variables are not eliminated if it produces a resolvent with a length above this limit. -1 means no limit", 20,   IntRange(-1, INT64_MAX));
static IntOption  opt_subsumption_lim    (_cat, "sub-lim", "Do not check if subsumption against a clause larger than this. -1 means no limit.", 1000, IntRange(-1, INT64_MAX));


//=================================================================================================
// Constructor/Destructor:


SimpSolver::SimpSolver() :
    grow               (opt_grow)
  , clause_lim         (opt_clause_lim)
  , subsumption_lim    (opt_subsumption_lim)
  , use_asymm          (opt_use_asymm)
  , use_rcheck         (opt_use_rcheck)
  , use_elim           (opt_use_elim)
  , merges             (0)
  , asymm_lits         (0)
  , elimorder          (1)
  , use_simplification (true)
  , elim_heap          (ElimLt(n_occ))
  , bwdsub_assigns     (0)
{
    vec<Lit> dummy(1,lit_Undef);
    bwdsub_tmpunit     = Clause_new(dummy);
    remove_satisfied   = false;
    extra_clause_field = true;
}


SimpSolver::~SimpSolver()
{
    free(bwdsub_tmpunit);
}


Var SimpSolver::newVar(bool sign, bool dvar) {
    Var v = Solver::newVar(sign, dvar);

    if (use_simplification){
        n_occ     .push(0);
        n_occ     .push(0);
        occurs    .push();
        frozen    .push((char)false);
        eliminated.push((char)false);
        touched   .push(0);
        elim_heap .insert(v);
    }
    return v; }



bool SimpSolver::solve(const vec<Lit>& assumps, bool do_simp, bool turn_off_simp) {
    vec<Var> extra_frozen;
    bool     result = true;

    do_simp &= use_simplification;

    if (do_simp){
        // Assumptions must be temporarily frozen to run variable elimination:
        for (int i = 0; i < assumps.size(); i++){
            Var v = var(assumps[i]);

            // If an assumption has been eliminated, remember it.
            assert(!isEliminated(v));

            if (!frozen[v]){
                // Freeze and store.
                setFrozen(v, true);
                extra_frozen.push(v);
            } }

        result = eliminate(turn_off_simp);
    }

    if (result)
        result = Solver::solve(assumps);
    else if (verbosity >= 1)
        printf("===============================================================================\n");

    if (result) 
        extendModel();

    if (do_simp)
        // Unfreeze the assumptions that were frozen:
        for (int i = 0; i < extra_frozen.size(); i++)
            setFrozen(extra_frozen[i], false);

    return result;
}



bool SimpSolver::addClause(const vec<Lit>& ps)
{
#ifndef NDEBUG
    for (int i = 0; i < ps.size(); i++)
        assert(!isEliminated(var(ps[i])));
#endif

    int nclauses = clauses.size();

    if (use_rcheck && implied(ps))
        return true;

    if (!Solver::addClause(ps))
        return false;

    if (use_simplification && clauses.size() == nclauses + 1){
        Clause& c = *clauses.last();

        subsumption_queue.insert(&c);

        for (int i = 0; i < c.size(); i++){
            occurs[var(c[i])].push(&c);
            n_occ[toInt(c[i])]++;
            touched[var(c[i])] = 1;
            if (elim_heap.inHeap(var(c[i])))
                elim_heap.increase(var(c[i]));
        }
    }

    return true;
}


void SimpSolver::removeClause(Clause& c)
{
    if (use_simplification)
        for (int i = 0; i < c.size(); i++){
            n_occ[toInt(c[i])]--;
            updateElimHeap(var(c[i]));
        }

    detachClause(c);
    c.mark(1);
}


bool SimpSolver::strengthenClause(Clause& c, Lit l)
{
    assert(decisionLevel() == 0);
    assert(use_simplification);

    // FIX: this is too inefficient but would be nice to have (properly implemented)
    // if (!find(subsumption_queue, &c))
    subsumption_queue.insert(&c);

    if (c.size() == 2){
        removeClause(c);
        c.strengthen(l);
    }else{
        detachClause(c);
        c.strengthen(l);
        attachClause(c);
        remove(occurs[var(l)], &c);
        n_occ[toInt(l)]--;
        updateElimHeap(var(l));
    }

    return c.size() == 1 ? enqueue(c[0]) && propagate() == NULL : true;
}


// Returns FALSE if clause is always satisfied ('out_clause' should not be used).
bool SimpSolver::merge(const Clause& _ps, const Clause& _qs, Var v, vec<Lit>& out_clause)
{
    merges++;
    out_clause.clear();

    bool  ps_smallest = _ps.size() < _qs.size();
    const Clause& ps  =  ps_smallest ? _qs : _ps;
    const Clause& qs  =  ps_smallest ? _ps : _qs;

    for (int i = 0; i < qs.size(); i++){
        if (var(qs[i]) != v){
            for (int j = 0; j < ps.size(); j++)
                if (var(ps[j]) == var(qs[i]))
                    if (ps[j] == ~qs[i])
                        return false;
                    else
                        goto next;
            out_clause.push(qs[i]);
        }
        next:;
    }

    for (int i = 0; i < ps.size(); i++)
        if (var(ps[i]) != v)
            out_clause.push(ps[i]);

    return true;
}


// Returns FALSE if clause is always satisfied.
bool SimpSolver::merge(const Clause& _ps, const Clause& _qs, Var v, int& size)
{
    merges++;

    bool  ps_smallest = _ps.size() < _qs.size();
    const Clause& ps  =  ps_smallest ? _qs : _ps;
    const Clause& qs  =  ps_smallest ? _ps : _qs;
    const Lit*  __ps  = (const Lit*)ps;
    const Lit*  __qs  = (const Lit*)qs;

    size = ps.size()-1;

    for (int i = 0; i < qs.size(); i++){
        if (var(__qs[i]) != v){
            for (int j = 0; j < ps.size(); j++)
                if (var(__ps[j]) == var(__qs[i]))
                    if (__ps[j] == ~__qs[i])
                        return false;
                    else
                        goto next;
            size++;
        }
        next:;
    }

    return true;
}


void SimpSolver::gatherTouchedClauses()
{
    //fprintf(stderr, "Gathering clauses for backwards subsumption\n");
    int ntouched = 0;
    for (int i = 0; i < touched.size(); i++)
        if (touched[i]){
            const vec<Clause*>& cs = getOccurs(i);
            ntouched++;
            for (int j = 0; j < cs.size(); j++)
                if (cs[j]->mark() == 0){
                    subsumption_queue.insert(cs[j]);
                    cs[j]->mark(2);
                }
            touched[i] = 0;
        }

    //fprintf(stderr, "Touched variables %d of %d yields %d clauses to check\n", ntouched, touched.size(), clauses.size());
    for (int i = 0; i < subsumption_queue.size(); i++)
        subsumption_queue[i]->mark(0);
}


bool SimpSolver::implied(const vec<Lit>& c)
{
    assert(decisionLevel() == 0);

    trail_lim.push(trail.size());
    for (int i = 0; i < c.size(); i++)
        if (value(c[i]) == l_True){
            cancelUntil(0);
            return false;
        }else if (value(c[i]) != l_False){
            assert(value(c[i]) == l_Undef);
            uncheckedEnqueue(~c[i]);
        }

    bool result = propagate() != NULL;
    cancelUntil(0);
    return result;
}


// Backward subsumption + backward subsumption resolution
bool SimpSolver::backwardSubsumptionCheck(bool verbose)
{
    int cnt = 0;
    int subsumed = 0;
    int deleted_literals = 0;
    assert(decisionLevel() == 0);

    while (subsumption_queue.size() > 0 || bwdsub_assigns < trail.size()){

        // Check top-level assignments by creating a dummy clause and placing it in the queue:
        if (subsumption_queue.size() == 0 && bwdsub_assigns < trail.size()){
            Lit l = trail[bwdsub_assigns++];
            (*bwdsub_tmpunit)[0] = l;
            bwdsub_tmpunit->calcAbstraction();
            subsumption_queue.insert(bwdsub_tmpunit); }

        Clause&  c = *subsumption_queue.peek(); subsumption_queue.pop();

        if (c.mark()) continue;

        if (verbose && verbosity >= 2 && cnt++ % 1000 == 0)
            printf("subsumption left: %10d (%10d subsumed, %10d deleted literals)\r", subsumption_queue.size(), subsumed, deleted_literals);

        assert(c.size() > 1 || value(c[0]) == l_True);    // Unit-clauses should have been propagated before this point.

        // Find best variable to scan:
        Var best = var(c[0]);
        for (int i = 1; i < c.size(); i++)
            if (occurs[var(c[i])].size() < occurs[best].size())
                best = var(c[i]);

        // Search all candidates:
        vec<Clause*>& _cs = getOccurs(best);
        Clause**       cs = (Clause**)_cs;

        for (int j = 0; j < _cs.size(); j++)
            if (c.mark())
                break;
            else if (!cs[j]->mark() &&  cs[j] != &c && (subsumption_lim == -1 || cs[j]->size() < subsumption_lim)){
                Lit l = c.subsumes(*cs[j]);

                if (l == lit_Undef)
                    subsumed++, removeClause(*cs[j]);
                else if (l != lit_Error){
                    deleted_literals++;

                    if (!strengthenClause(*cs[j], ~l))
                        return false;

                    // Did current candidate get deleted from cs? Then check candidate at index j again:
                    if (var(l) == best)
                        j--;
                }
            }
    }

    return true;
}


bool SimpSolver::asymm(Var v, Clause& c)
{
    assert(decisionLevel() == 0);

    if (c.mark() || satisfied(c)) return true;

    trail_lim.push(trail.size());
    Lit l = lit_Undef;
    for (int i = 0; i < c.size(); i++)
        if (var(c[i]) != v && value(c[i]) != l_False)
            uncheckedEnqueue(~c[i]);
        else
            l = c[i];

    if (propagate() != NULL){
        cancelUntil(0);
        asymm_lits++;
        if (!strengthenClause(c, l))
            return false;
    }else
        cancelUntil(0);

    return true;
}


bool SimpSolver::asymmVar(Var v)
{
    assert(use_simplification);

    const vec<Clause*>& cls = getOccurs(v);

    if (value(v) != l_Undef || cls.size() == 0)
        return true;

    for (int i = 0; i < cls.size(); i++)
        if (!asymm(v, *cls[i]))
            return false;

    return backwardSubsumptionCheck();
}


static void mkElimClause(vec<uint32_t>& elimclauses, Lit x)
{
    elimclauses.push(toInt(x));
    elimclauses.push(1);
}


static void mkElimClause(vec<uint32_t>& elimclauses, Var v, Clause& c)
{
    int first = elimclauses.size();
    int v_pos = -1;

    // Copy clause to elimclauses-vector. Remember position where the
    // variable 'v' occurs:
    for (int i = 0; i < c.size(); i++){
        elimclauses.push(toInt(c[i]));
        if (var(c[i]) == v)
            v_pos = i + first;
    }
    assert(v_pos != -1);

    // Swap the first literal with the 'v' literal, so that the literal
    // containing 'v' will occur first in the clause:
    uint32_t tmp = elimclauses[v_pos];
    elimclauses[v_pos] = elimclauses[first];
    elimclauses[first] = tmp;

    // Store the length of the clause last:
    elimclauses.push(c.size());
}



bool SimpSolver::eliminateVar(Var v)
{
    assert(!frozen[v]);
    assert(!isEliminated(v));
    assert(value(v) == l_Undef);

    // Split the occurrences into positive and negative:
    //
    const vec<Clause*>& cls = getOccurs(v);
    vec<Clause*>  pos, neg;
    for (int i = 0; i < cls.size(); i++)
        (find(*cls[i], mkLit(v)) ? pos : neg).push(cls[i]);

    // Check wether the increase in number of clauses stays within the allowed ('grow'). Moreover, no
    // clause must exceed the limit on the maximal clause size (if it is set):
    //
    int cnt         = 0;
    int clause_size = 0;

    for (int i = 0; i < pos.size(); i++)
        for (int j = 0; j < neg.size(); j++)
            if (merge(*pos[i], *neg[j], v, clause_size) && 
                (++cnt > cls.size() + grow || (clause_lim != -1 && clause_size > clause_lim)))
                return true;

    // Delete and store old clauses:
    eliminated[v] = true;
    setDecisionVar(v, false);
    if (pos.size() > neg.size()){
        for (int i = 0; i < neg.size(); i++)
            mkElimClause(elimclauses, v, *neg[i]);
        mkElimClause(elimclauses, mkLit(v));
    }else{
        for (int i = 0; i < pos.size(); i++)
            mkElimClause(elimclauses, v, *pos[i]);
        mkElimClause(elimclauses, ~mkLit(v));
    }

    for (int i = 0; i < cls.size(); i++)
        removeClause(*cls[i]); 

    // Produce clauses in cross product:
    vec<Lit> resolvent;
    for (int i = 0; i < pos.size(); i++)
        for (int j = 0; j < neg.size(); j++)
            if (merge(*pos[i], *neg[j], v, resolvent) && !addClause(resolvent))
                return false;

    // Free occurs list for this variable:
    occurs[v].clear(true);
    
    // Free watchers lists for this variable, if possible:
    if (watches[toInt( mkLit(v))].size() == 0) watches[toInt( mkLit(v))].clear(true);
    if (watches[toInt(~mkLit(v))].size() == 0) watches[toInt(~mkLit(v))].clear(true);

    return backwardSubsumptionCheck();
}


bool SimpSolver::substitute(Var v, Lit x)
{
    assert(!frozen[v]);
    assert(!isEliminated(v));
    assert(value(v) == l_Undef);

    if (!ok) return false;

    eliminated[v] = true;
    setDecisionVar(v, false);
    const vec<Clause*>& cls = getOccurs(v);
    
    vec<Lit> subst_clause;
    for (int i = 0; i < cls.size(); i++){
        Clause& c = *cls[i];

        subst_clause.clear();
        for (int j = 0; j < c.size(); j++){
            Lit p = c[j];
            subst_clause.push(var(p) == v ? x ^ sign(p) : p);
        }

        removeClause(c);

        if (!addClause(subst_clause))
            return ok = false;
    }

    return true;
}


void SimpSolver::extendModel()
{
    int i, j;
    Lit x;

    for (i = elimclauses.size()-1; i > 0; i -= j){
        for (j = elimclauses[i--]; j > 1; j--, i--)
            if (modelValue(toLit(elimclauses[i])) != l_False)
                goto next;

        x = toLit(elimclauses[i]);
        model[var(x)] = lbool(!sign(x));
    next:;
    }
}


bool SimpSolver::eliminate(bool turn_off_elim)
{
    if (!simplify())
        return false;
    else if (!use_simplification)
        return true;

    // Main simplification loop:
    //
    while (subsumption_queue.size() > 0 || bwdsub_assigns < trail.size() || elim_heap.size() > 0){

        if (!backwardSubsumptionCheck(true))
            return ok = false;

        for (int cnt = 0; !elim_heap.empty(); cnt++){
            Var elim = elim_heap.removeMin();

            if (isEliminated(elim) || value(elim) != l_Undef) continue;

            if (verbosity >= 2 && cnt % 100 == 0)
                printf("elimination left: %10d\r", elim_heap.size());

            if (use_asymm){
                // Temporarily freeze variable. Otherwise, it would immediately end up on the queue again:
                bool was_frozen = frozen[elim];
                frozen[elim] = true;
                if (!asymmVar(elim))
                    return ok = false;
                frozen[elim] = was_frozen; }

            // At this point, the variable may have been set by assymetric branching, so check it
            // again. Also, don't eliminate frozen variables:
            if (use_elim && value(elim) == l_Undef && !frozen[elim] && !eliminateVar(elim))
                return ok = false;
        }

        assert(subsumption_queue.size() == 0);
        gatherTouchedClauses();
    }

    // Cleanup:
    cleanUpClauses();
    rebuildOrderHeap();

    // If no more simplification is needed, free all simplification-related data structures:
    if (turn_off_elim){
        use_simplification = false;
        touched.clear(true);
        occurs.clear(true);
        n_occ.clear(true);
        subsumption_queue.clear(true);
        elim_heap.clear(true);
        remove_satisfied = true;
        extra_clause_field = false;
    }

    if (verbosity >= 1 && elimclauses.size() > 0)
        printf("|  Eliminated clauses:     %10.2f Mb                                      |\n", 
               double(elimclauses.size() * sizeof(uint32_t)) / (1024*1024));

    return true;
}


void SimpSolver::cleanUpClauses()
{
    int      i , j;
    vec<Var> dirty;
    for (i = 0; i < clauses.size(); i++)
        if (clauses[i]->mark() == 1){
            Clause& c = *clauses[i];
            for (int k = 0; k < c.size(); k++)
                if (!seen[var(c[k])]){
                    seen[var(c[k])] = 1;
                    dirty.push(var(c[k]));
                } }

    for (i = 0; i < dirty.size(); i++){
        cleanOcc(dirty[i]);
        seen[dirty[i]] = 0; }

    for (i = j = 0; i < clauses.size(); i++)
        if (clauses[i]->mark() == 1)
            free(clauses[i]);
        else
            clauses[j++] = clauses[i];
    clauses.shrink(i - j);
}


//=================================================================================================
// Convert to DIMACS:


static Var mapVar(Var x, vec<Var>& map, Var& max)
{
    if (map.size() <= x || map[x] == -1){
        map.growTo(x+1, -1);
        map[x] = max++;
    }
    return map[x];
}


void SimpSolver::toDimacs(FILE* f, Clause& c, vec<Var>& map, Var& max)
{
    if (satisfied(c)) return;

    for (int i = 0; i < c.size(); i++)
        if (value(c[i]) != l_False)
            fprintf(f, "%s%d ", sign(c[i]) ? "-" : "", mapVar(var(c[i]), map, max)+1);
    fprintf(f, "0\n");
}


void SimpSolver::toDimacs(const char* file)
{
    assert(decisionLevel() == 0);
    FILE* f = fopen(file, "wr");
    if (f != NULL){
        vec<Var> map; Var max = 0;

        // Cannot use removeClauses here because it is not safe
        // to deallocate them at this point. Could be improved.
        int cnt = 0;
        for (int i = 0; i < clauses.size(); i++)
            if (!satisfied(*clauses[i]))
                cnt++;
        
        for (int i = 0; i < clauses.size(); i++)
            if (!satisfied(*clauses[i])){
                Clause& c = *clauses[i];
                for (int j = 0; j < c.size(); j++)
                    if (value(c[j]) != l_False)
                        mapVar(var(c[j]), map, max);
            }

        fprintf(f, "p cnf %d %d\n", max, cnt);

        for (int i = 0; i < clauses.size(); i++)
            toDimacs(f, *clauses[i], map, max);

        fprintf(stderr, "Wrote %d clauses with %d variables.\n", cnt, max);
        fclose(f);
    }else
        fprintf(stderr, "could not open file %s\n", file);
}
