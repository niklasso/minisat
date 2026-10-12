/***************************************************************************************[Solver.cc]
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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "minisat/mtl/Alg.h"
#include "minisat/mtl/Sort.h"
#include "minisat/utils/System.h"
#include "minisat/core/Solver.h"

using namespace Minisat;

//=================================================================================================
// Options:


static const char* _cat = "CORE";

static DoubleOption  opt_var_decay         (_cat, "var-decay",   "The variable activity decay factor",            0.95,     DoubleRange(0, false, 1, false));
static DoubleOption  opt_clause_decay      (_cat, "cla-decay",   "The clause activity decay factor",              0.999,    DoubleRange(0, false, 1, false));
static DoubleOption  opt_random_var_freq   (_cat, "rnd-freq",    "The frequency with which the decision heuristic tries to choose a random variable", 0, DoubleRange(0, true, 1, true));
static DoubleOption  opt_random_seed       (_cat, "rnd-seed",    "Used by the random variable selection",         91648253, DoubleRange(0, false, HUGE_VAL, false));
static IntOption     opt_ccmin_mode        (_cat, "ccmin-mode",  "Controls conflict clause minimization (0=none, 1=basic, 2=deep)", 2, IntRange(0, 2));
static IntOption     opt_phase_saving      (_cat, "phase-saving", "Controls the level of phase saving (0=none, 1=limited, 2=full)", 2, IntRange(0, 2));
static BoolOption    opt_rnd_init_act      (_cat, "rnd-init",    "Randomize the initial activity", false);
static BoolOption    opt_luby_restart      (_cat, "luby",        "Use the Luby restart sequence", true);
static IntOption     opt_restart_first     (_cat, "rfirst",      "The base restart interval", 100, IntRange(1, INT32_MAX));
static DoubleOption  opt_restart_inc       (_cat, "rinc",        "Restart interval increase factor", 2, DoubleRange(1, false, HUGE_VAL, false));
static DoubleOption  opt_garbage_frac      (_cat, "gc-frac",     "The fraction of wasted memory allowed before a garbage collection is triggered",  0.20, DoubleRange(0, false, HUGE_VAL, false));
static IntOption     opt_min_learnts_lim   (_cat, "min-learnts", "Minimum learnt clause limit",  0, IntRange(0, INT32_MAX));


//=================================================================================================
// Constructor/Destructor:


Solver::Solver() :

    // Parameters (user settable):
    //
    verbosity        (0)
  , var_decay        (opt_var_decay)
  , clause_decay     (opt_clause_decay)
  , random_var_freq  (opt_random_var_freq)
  , random_seed      (opt_random_seed)
  , luby_restart     (opt_luby_restart)
  , ccmin_mode       (opt_ccmin_mode)
  , phase_saving     (opt_phase_saving)
  , rnd_pol          (false)
  , rnd_init_act     (opt_rnd_init_act)
  , garbage_frac     (opt_garbage_frac)
  , min_learnts_lim  (opt_min_learnts_lim)
  , restart_first    (opt_restart_first)
  , restart_inc      (opt_restart_inc)

    // Parameters (the rest):
    //
  , learntsize_factor((double)1/(double)3), learntsize_inc(1.1)

    // Parameters (experimental):
    //
  , learntsize_adjust_start_confl (100)
  , learntsize_adjust_inc         (1.5)

    // Statistics: (formerly in 'SolverStats')
    //
  , solves(0), starts(0), decisions(0), rnd_decisions(0), propagations(0), conflicts(0)
  , dec_vars(0), num_clauses(0), num_learnts(0), clauses_literals(0), learnts_literals(0), max_literals(0), tot_literals(0)
  , ext_notifications(0), ext_propagations(0), ext_reasons(0), ext_clauses(0), ext_conflicts(0), ext_decisions(0), ext_checks(0)

  , watches            (WatcherDeleted(ca))
  , order_heap         (VarOrderLt(activity))
  , ok                 (true)
  , cla_inc            (1)
  , var_inc            (1)
  , qhead              (0)
  , simpDB_assigns     (-1)
  , simpDB_props       (0)
  , progress_estimate  (0)
  , remove_satisfied   (true)
  , next_var           (0)

    // Resource constraints:
    //
  , conflict_budget    (-1)
  , propagation_budget (-1)
  , asynch_interrupt   (false)
  , terminator         (NULL)

    // External propagation:
    //
  , propagator         (NULL)
  , notified           (0)
  , notified_level     (0)
  , in_explanation     (false)
  , backtrack_allowed  (false)
  , ext_rejected       (false)
{}


Solver::~Solver()
{
}


//=================================================================================================
// Minor methods:


// Creates a new SAT variable in the solver. If 'decision' is cleared, variable will not be
// used as a decision variable (NOTE! This has effects on the meaning of a SATISFIABLE result).
//
Var Solver::newVar(lbool upol, bool dvar)
{
    Var v;
    if (free_vars.size() > 0){
        v = free_vars.last();
        free_vars.pop();
    }else
        v = next_var++;

    watches  .init(mkLit(v, false));
    watches  .init(mkLit(v, true ));
    assigns  .insert(v, l_Undef);
    vardata  .insert(v, mkVarData(CRef_Undef, 0));
    activity .insert(v, rnd_init_act ? drand(random_seed) * 0.00001 : 0);
    seen     .insert(v, 0);
    polarity .insert(v, true);
    user_pol .insert(v, upol);
    observed .insert(v, 0);
    decision .reserve(v);
    trail    .capacity(v+1);
    setDecisionVar(v, dvar);
    return v;
}


// Note: at the moment, only unassigned variable will be released (this is to avoid duplicate
// releases of the same variable).
void Solver::releaseVar(Lit l)
{
    if (observed[var(l)])
        remove_observed_var(var(l));
    if (value(l) == l_Undef){
        addClause(l);
        released_vars.push(var(l));
    }
}


bool Solver::addClause_(vec<Lit>& ps)
{
    assert(decisionLevel() == 0);
    if (!ok) return false;

    // Check if clause is satisfied and remove false/duplicate literals:
    sort(ps);
    Lit p; int i, j;
    for (i = j = 0, p = lit_Undef; i < ps.size(); i++)
        if (value(ps[i]) == l_True || ps[i] == ~p)
            return true;
        else if (value(ps[i]) != l_False && ps[i] != p)
            ps[j++] = p = ps[i];
    ps.shrink(i - j);

    if (ps.size() == 0)
        return ok = false;
    else if (ps.size() == 1){
        uncheckedEnqueue(ps[0]);
        return ok = (propagate() == CRef_Undef);
    }else{
        CRef cr = ca.alloc(ps, false);
        clauses.push(cr);
        attachClause(cr);
    }

    return true;
}


void Solver::attachClause(CRef cr){
    const Clause& c = ca[cr];
    assert(c.size() > 1);
    watches[~c[0]].push(Watcher(cr, c[1]));
    watches[~c[1]].push(Watcher(cr, c[0]));
    if (c.learnt()) num_learnts++, learnts_literals += c.size();
    else            num_clauses++, clauses_literals += c.size();
}


void Solver::detachClause(CRef cr, bool strict){
    const Clause& c = ca[cr];
    assert(c.size() > 1);
    
    // Strict or lazy detaching:
    if (strict){
        remove(watches[~c[0]], Watcher(cr, c[1]));
        remove(watches[~c[1]], Watcher(cr, c[0]));
    }else{
        watches.smudge(~c[0]);
        watches.smudge(~c[1]);
    }

    if (c.learnt()) num_learnts--, learnts_literals -= c.size();
    else            num_clauses--, clauses_literals -= c.size();
}


void Solver::removeClause(CRef cr) {
    Clause& c = ca[cr];
    detachClause(cr);
    // Don't leave pointers to free'd memory!
    if (locked(c)) vardata[var(c[0])].reason = CRef_Undef;
    c.mark(1); 
    ca.free(cr);
}


bool Solver::satisfied(const Clause& c) const {
    for (int i = 0; i < c.size(); i++)
        if (value(c[i]) == l_True)
            return true;
    return false; }


// Revert to the state at given level (keeping all assignment at 'level' but not beyond).
//
void Solver::cancelUntil(int level) {
    if (decisionLevel() > level){
        for (int c = trail.size()-1; c >= trail_lim[level]; c--){
            Var      x  = var(trail[c]);
            assigns [x] = l_Undef;
            if (phase_saving > 1 || (phase_saving == 1 && c > trail_lim.last()))
                polarity[x] = sign(trail[c]);
            insertVarOrder(x); }
        qhead = trail_lim[level];
        trail.shrink(trail.size() - trail_lim[level]);
        trail_lim.shrink(trail_lim.size() - level);

        // Whatever was undone and never reported stays unreported; the propagator hears only of
        // levels it was told were opened.
        if (notified > trail.size()) notified = trail.size();
        if (propagator != NULL && level < notified_level){
            notified_level = level;
            propagator->notify_backtrack(level); }
    } }


//=================================================================================================
// Major methods:


Lit Solver::pickBranchLit()
{
    Var next = var_Undef;

    // Random decision:
    if (drand(random_seed) < random_var_freq && !order_heap.empty()){
        next = order_heap[irand(random_seed,order_heap.size())];
        if (value(next) == l_Undef && decision[next])
            rnd_decisions++; }

    // Activity based decision:
    while (next == var_Undef || value(next) != l_Undef || !decision[next])
        if (order_heap.empty()){
            next = var_Undef;
            break;
        }else
            next = order_heap.removeMin();

    return next == var_Undef ? lit_Undef : polarityFor(next);
}


// Choose polarity based on different polarity modes (global or per-variable):
Lit Solver::polarityFor(Var v)
{
    if (user_pol[v] != l_Undef)
        return mkLit(v, user_pol[v] == l_True);
    else if (rnd_pol)
        return mkLit(v, drand(random_seed) < 0.5);
    else
        return mkLit(v, polarity[v]);
}


/*_________________________________________________________________________________________________
|
|  analyze : (confl : Clause*) (out_learnt : vec<Lit>&) (out_btlevel : int&)  ->  [void]
|  
|  Description:
|    Analyze conflict and produce a reason clause.
|  
|    Pre-conditions:
|      * 'out_learnt' is assumed to be cleared.
|      * Current decision level must be greater than root level.
|  
|    Post-conditions:
|      * 'out_learnt[0]' is the asserting literal at level 'out_btlevel'.
|      * If out_learnt.size() > 1 then 'out_learnt[1]' has the greatest decision level of the 
|        rest of literals. There may be others from the same level though.
|  
|________________________________________________________________________________________________@*/
void Solver::analyze(CRef confl, vec<Lit>& out_learnt, int& out_btlevel)
{
    int pathC = 0;
    Lit p     = lit_Undef;

    // Generate conflict clause:
    //
    out_learnt.push();      // (leave room for the asserting literal)
    int index   = trail.size() - 1;

    do{
        assert(confl != CRef_Undef); // (otherwise should be UIP)
        Clause& c = ca[confl];

        if (c.learnt())
            claBumpActivity(c);

        for (int j = (p == lit_Undef) ? 0 : 1; j < c.size(); j++){
            Lit q = c[j];

            if (!seen[var(q)] && level(var(q)) > 0){
                varBumpActivity(var(q));
                seen[var(q)] = 1;
                if (level(var(q)) >= decisionLevel())
                    pathC++;
                else
                    out_learnt.push(q);
            }
        }
        
        // Select next clause to look at. The UIP's reason is never read, so a lazy one is left
        // unexplained:
        while (!seen[var(trail[index--])]);
        p     = trail[index+1];
        seen[var(p)] = 0;
        pathC--;
        if (pathC > 0)
            confl = reasonClause(var(p));

    }while (pathC > 0);
    out_learnt[0] = ~p;

    // Simplify conflict clause:
    //
    int i, j;
    out_learnt.copyTo(analyze_toclear);
    if (ccmin_mode == 2){
        for (i = j = 1; i < out_learnt.size(); i++)
            if (reason(var(out_learnt[i])) == CRef_Undef || !litRedundant(out_learnt[i]))
                out_learnt[j++] = out_learnt[i];
        
    }else if (ccmin_mode == 1){
        for (i = j = 1; i < out_learnt.size(); i++){
            Var x = var(out_learnt[i]);

            if (reason(x) == CRef_Undef)
                out_learnt[j++] = out_learnt[i];
            else{
                Clause& c = ca[reasonClause(x)];
                for (int k = 1; k < c.size(); k++)
                    if (!seen[var(c[k])] && level(var(c[k])) > 0){
                        out_learnt[j++] = out_learnt[i];
                        break; }
            }
        }
    }else
        i = j = out_learnt.size();

    max_literals += out_learnt.size();
    out_learnt.shrink(i - j);
    tot_literals += out_learnt.size();

    // Find correct backtrack level:
    //
    if (out_learnt.size() == 1)
        out_btlevel = 0;
    else{
        int max_i = 1;
        // Find the first literal assigned at the next-highest level:
        for (int i = 2; i < out_learnt.size(); i++)
            if (level(var(out_learnt[i])) > level(var(out_learnt[max_i])))
                max_i = i;
        // Swap-in this literal at index 1:
        Lit p             = out_learnt[max_i];
        out_learnt[max_i] = out_learnt[1];
        out_learnt[1]     = p;
        out_btlevel       = level(var(p));
    }

    for (int j = 0; j < analyze_toclear.size(); j++) seen[var(analyze_toclear[j])] = 0;    // ('seen[]' is now cleared)
}


// Check if 'p' can be removed from a conflict clause.
bool Solver::litRedundant(Lit p)
{
    enum { seen_undef = 0, seen_source = 1, seen_removable = 2, seen_failed = 3 };
    assert(seen[var(p)] == seen_undef || seen[var(p)] == seen_source);
    assert(reason(var(p)) != CRef_Undef);

    Clause*               c     = &ca[reasonClause(var(p))];
    vec<ShrinkStackElem>& stack = analyze_stack;
    stack.clear();

    for (uint32_t i = 1; ; i++){
        if (i < (uint32_t)c->size()){
            // Checking 'p'-parents 'l':
            Lit l = (*c)[i];
            
            // Variable at level 0 or previously removable:
            if (level(var(l)) == 0 || seen[var(l)] == seen_source || seen[var(l)] == seen_removable){
                continue; }
            
            // Check variable can not be removed for some local reason:
            if (reason(var(l)) == CRef_Undef || seen[var(l)] == seen_failed){
                stack.push(ShrinkStackElem(0, p));
                for (int i = 0; i < stack.size(); i++)
                    if (seen[var(stack[i].l)] == seen_undef){
                        seen[var(stack[i].l)] = seen_failed;
                        analyze_toclear.push(stack[i].l);
                    }
                    
                return false;
            }

            // Recursively check 'l'. Explaining it may move clause memory, so 'c' is taken after:
            stack.push(ShrinkStackElem(i, p));
            i  = 0;
            p  = l;
            c  = &ca[reasonClause(var(p))];
        }else{
            // Finished with current element 'p' and reason 'c':
            if (seen[var(p)] == seen_undef){
                seen[var(p)] = seen_removable;
                analyze_toclear.push(p);
            }

            // Terminate with success if stack is empty:
            if (stack.size() == 0) break;
            
            // Continue with top element on stack:
            i  = stack.last().i;
            p  = stack.last().l;
            c  = &ca[reasonClause(var(p))];

            stack.pop();
        }
    }

    return true;
}


/*_________________________________________________________________________________________________
|
|  analyzeFinal : (p : Lit)  ->  [void]
|  
|  Description:
|    Specialized analysis procedure to express the final conflict in terms of assumptions.
|    Calculates the (possibly empty) set of assumptions that led to the assignment of 'p', and
|    stores the result in 'out_conflict'.
|________________________________________________________________________________________________@*/
void Solver::analyzeFinal(Lit p, LSet& out_conflict)
{
    out_conflict.clear();
    out_conflict.insert(p);

    if (decisionLevel() == 0)
        return;

    seen[var(p)] = 1;

    for (int i = trail.size()-1; i >= trail_lim[0]; i--){
        Var x = var(trail[i]);
        if (seen[x]){
            if (reason(x) == CRef_Undef){
                assert(level(x) > 0);
                out_conflict.insert(~trail[i]);
            }else{
                Clause& c = ca[reasonClause(x)];
                for (int j = 1; j < c.size(); j++)
                    if (level(var(c[j])) > 0)
                        seen[var(c[j])] = 1;
            }
            seen[x] = 0;
        }
    }

    seen[var(p)] = 0;
}


void Solver::uncheckedEnqueue(Lit p, CRef from)
{
    assert(value(p) == l_Undef);
    assigns[var(p)] = lbool(!sign(p));
    vardata[var(p)] = mkVarData(from, decisionLevel());
    trail.push_(p);
}


/*_________________________________________________________________________________________________
|
|  propagate : [void]  ->  [Clause*]
|  
|  Description:
|    Propagates all enqueued facts. If a conflict arises, the conflicting clause is returned,
|    otherwise CRef_Undef.
|  
|    Post-conditions:
|      * the propagation queue is empty, even if there was a conflict.
|________________________________________________________________________________________________@*/
CRef Solver::propagate()
{
    CRef    confl     = CRef_Undef;
    int     num_props = 0;

    while (qhead < trail.size()){
        Lit            p   = trail[qhead++];     // 'p' is enqueued fact to propagate.
        vec<Watcher>&  ws  = watches.lookup(p);
        Watcher        *i, *j, *end;
        num_props++;

        for (i = j = (Watcher*)ws, end = i + ws.size();  i != end;){
            // Try to avoid inspecting the clause:
            Lit blocker = i->blocker;
            if (value(blocker) == l_True){
                *j++ = *i++; continue; }

            // Make sure the false literal is data[1]:
            CRef     cr        = i->cref;
            Clause&  c         = ca[cr];
            Lit      false_lit = ~p;
            if (c[0] == false_lit)
                c[0] = c[1], c[1] = false_lit;
            assert(c[1] == false_lit);
            i++;

            // If 0th watch is true, then clause is already satisfied.
            Lit     first = c[0];
            Watcher w     = Watcher(cr, first);
            if (first != blocker && value(first) == l_True){
                *j++ = w; continue; }

            // Look for new watch:
            for (int k = 2; k < c.size(); k++)
                if (value(c[k]) != l_False){
                    c[1] = c[k]; c[k] = false_lit;
                    watches[~c[1]].push(w);
                    goto NextClause; }

            // Did not find watch -- clause is unit under assignment:
            *j++ = w;
            if (value(first) == l_False){
                confl = cr;
                qhead = trail.size();
                // Copy the remaining watches:
                while (i < end)
                    *j++ = *i++;
            }else
                uncheckedEnqueue(first, cr);

        NextClause:;
        }
        ws.shrink(i - j);
    }
    propagations += num_props;
    simpDB_props -= num_props;

    return confl;
}


/*_________________________________________________________________________________________________
|
|  reduceDB : ()  ->  [void]
|  
|  Description:
|    Remove half of the learnt clauses, minus the clauses locked by the current assignment. Locked
|    clauses are clauses that are reason to some assignment. Binary clauses are never removed.
|________________________________________________________________________________________________@*/
struct reduceDB_lt { 
    ClauseAllocator& ca;
    reduceDB_lt(ClauseAllocator& ca_) : ca(ca_) {}
    bool operator () (CRef x, CRef y) { 
        return ca[x].size() > 2 && (ca[y].size() == 2 || ca[x].activity() < ca[y].activity()); } 
};
void Solver::reduceDB()
{
    int     i, j;
    double  extra_lim = cla_inc / learnts.size();    // Remove any clause below this activity

    sort(learnts, reduceDB_lt(ca));
    // Don't delete binary or locked clauses. From the rest, delete clauses from the first half
    // and clauses with activity smaller than 'extra_lim':
    for (i = j = 0; i < learnts.size(); i++){
        Clause& c = ca[learnts[i]];
        if (c.size() > 2 && !locked(c) && (i < learnts.size() / 2 || c.activity() < extra_lim))
            removeClause(learnts[i]);
        else
            learnts[j++] = learnts[i];
    }
    learnts.shrink(i - j);
    checkGarbage();
}


void Solver::removeSatisfied(vec<CRef>& cs)
{
    int i, j;
    for (i = j = 0; i < cs.size(); i++){
        Clause& c = ca[cs[i]];
        if (satisfied(c))
            removeClause(cs[i]);
        else{
            // Trim clause:
            assert(value(c[0]) == l_Undef && value(c[1]) == l_Undef);
            for (int k = 2; k < c.size(); k++)
                if (value(c[k]) == l_False){
                    c[k--] = c[c.size()-1];
                    c.pop();
                }
            cs[j++] = cs[i];
        }
    }
    cs.shrink(i - j);
}


void Solver::rebuildOrderHeap()
{
    vec<Var> vs;
    for (Var v = 0; v < nVars(); v++)
        if (decision[v] && value(v) == l_Undef)
            vs.push(v);
    order_heap.build(vs);
}


/*_________________________________________________________________________________________________
|
|  simplify : [void]  ->  [bool]
|  
|  Description:
|    Simplify the clause database according to the current top-level assigment. Currently, the only
|    thing done here is the removal of satisfied clauses, but more things can be put here.
|________________________________________________________________________________________________@*/
bool Solver::simplify()
{
    assert(decisionLevel() == 0);

    if (!ok || propagate() != CRef_Undef)
        return ok = false;

    if (nAssigns() == simpDB_assigns || (simpDB_props > 0))
        return true;

    // Remove satisfied clauses:
    removeSatisfied(learnts);
    if (remove_satisfied){       // Can be turned off.
        removeSatisfied(clauses);

        // TODO: what todo in if 'remove_satisfied' is false?

        // Remove all released variables from the trail:
        for (int i = 0; i < released_vars.size(); i++){
            assert(seen[released_vars[i]] == 0);
            seen[released_vars[i]] = 1;
        }

        // The notification cursor moves with the entries it counts:
        int i, j, kept_notified = 0;
        for (i = j = 0; i < trail.size(); i++)
            if (seen[var(trail[i])] == 0){
                if (i < notified) kept_notified++;
                trail[j++] = trail[i]; }
        trail.shrink(i - j);
        notified = kept_notified;
        //printf("trail.size()= %d, qhead = %d\n", trail.size(), qhead);
        qhead = trail.size();

        for (int i = 0; i < released_vars.size(); i++)
            seen[released_vars[i]] = 0;

        // Released variables are now ready to be reused:
        append(released_vars, free_vars);
        released_vars.clear();
    }
    checkGarbage();
    rebuildOrderHeap();

    simpDB_assigns = nAssigns();
    simpDB_props   = clauses_literals + learnts_literals;   // (shouldn't depend on stats really, but it will do for now)

    return true;
}


/*_________________________________________________________________________________________________
|
|  search : (nof_conflicts : int) (params : const SearchParams&)  ->  [lbool]
|  
|  Description:
|    Search for a model the specified number of conflicts. 
|    NOTE! Use negative value for 'nof_conflicts' indicate infinity.
|  
|  Output:
|    'l_True' if a partial assigment that is consistent with respect to the clauseset is found. If
|    all variables are decision variables, this means that the clause set is satisfiable. 'l_False'
|    if the clause set is unsatisfiable. 'l_Undef' if the bound on number of conflicts is reached.
|________________________________________________________________________________________________@*/
lbool Solver::search(int nof_conflicts)
{
    assert(ok);
    int         backtrack_level;
    int         conflictC = 0;
    vec<Lit>    learnt_clause;
    CRef        ext_confl = CRef_Undef; // A conflict raised while the propagator judged a model.
    starts++;

    for (;;){
        CRef confl = ext_confl != CRef_Undef ? ext_confl : propagate();
        ext_confl  = CRef_Undef;
        if (confl == CRef_Undef && propagatorActive()){
            confl = propagateExternal();
            if (!ok) return l_False;
        }
        if (confl != CRef_Undef){
            // CONFLICT
            conflicts++; conflictC++;
            if (decisionLevel() == 0) return l_False;

            learnt_clause.clear();
            analyze(confl, learnt_clause, backtrack_level);
            cancelUntil(backtrack_level);

            if (learnt_clause.size() == 1){
                uncheckedEnqueue(learnt_clause[0]);
            }else{
                CRef cr = ca.alloc(learnt_clause, true);
                learnts.push(cr);
                attachClause(cr);
                claBumpActivity(ca[cr]);
                uncheckedEnqueue(learnt_clause[0], cr);
            }

            varDecayActivity();
            claDecayActivity();

            if (--learntsize_adjust_cnt == 0){
                learntsize_adjust_confl *= learntsize_adjust_inc;
                learntsize_adjust_cnt    = (int)learntsize_adjust_confl;
                max_learnts             *= learntsize_inc;

                if (verbosity >= 1)
                    printf("| %9d | %7d %8d %8d | %8d %8d %6.0f | %6.3f %% |\n", 
                           (int)conflicts, 
                           (int)dec_vars - (trail_lim.size() == 0 ? trail.size() : trail_lim[0]), nClauses(), (int)clauses_literals, 
                           (int)max_learnts, nLearnts(), (double)learnts_literals/nLearnts(), progressEstimate()*100);
            }

        }else{
            // NO CONFLICT
            if ((nof_conflicts >= 0 && conflictC >= nof_conflicts) || !withinBudget()){
                // Reached bound on number of conflicts:
                progress_estimate = progressEstimate();
                cancelUntil(0);
                return l_Undef; }

            // Simplify the set of problem clauses:
            if (decisionLevel() == 0 && !simplify())
                return l_False;

            if (learnts.size()-nAssigns() >= max_learnts)
                // Reduce the set of learnt clauses:
                reduceDB();

            Lit next = lit_Undef;
            while (decisionLevel() < assumptions.size()){
                // Perform user provided assumption:
                Lit p = assumptions[decisionLevel()];
                if (value(p) == l_True){
                    // Dummy decision level:
                    newDecisionLevel();
                }else if (value(p) == l_False){
                    analyzeFinal(~p, conflict);
                    return l_False;
                }else{
                    next = p;
                    break;
                }
            }

            if (next == lit_Undef && propagatorActive()){
                // The propagator's own decision, if it has one; if it backtracked instead, the
                // search goes back to propagating, and redoes the assumptions if it must:
                next = decideExternal();
                if (next == lit_Error) continue;
            }

            if (next == lit_Undef){
                // New variable decision:
                decisions++;
                next = pickBranchLit();

                // A model has to settle every variable the propagator observes, decision
                // variable or not:
                if (next == lit_Undef && propagator != NULL)
                    next = pickObservedLit();

                if (next == lit_Undef){
                    // Model found -- if the propagator agrees:
                    if (propagator == NULL)
                        return l_True;
                    lbool verdict = checkModelExternal(ext_confl);
                    if (!ok)
                        return l_False;
                    if (verdict == l_True)
                        return l_True;
                    if (verdict == l_False){
                        ext_rejected = true;
                        return l_Undef; }
                    continue;
                }

                if (propagatorActive() && propagator->advises_polarity && observed[var(next)]){
                    Lit advised = propagator->cb_decide_polarity(next);
                    requireExternal(advised == next || advised == ~next,
                                    "cb_decide_polarity() returned a literal of another variable");
                    next = advised;
                }
            }

            // Increase decision level and enqueue 'next'
            newDecisionLevel();
            uncheckedEnqueue(next);
        }
    }
}


double Solver::progressEstimate() const
{
    double  progress = 0;
    double  F = 1.0 / nVars();

    for (int i = 0; i <= decisionLevel(); i++){
        int beg = i == 0 ? 0 : trail_lim[i - 1];
        int end = i == decisionLevel() ? trail.size() : trail_lim[i];
        progress += pow(F, i) * (end - beg);
    }

    return progress / nVars();
}

/*
  Finite subsequences of the Luby-sequence:

  0: 1
  1: 1 1 2
  2: 1 1 2 1 1 2 4
  3: 1 1 2 1 1 2 4 1 1 2 1 1 2 4 8
  ...


 */

static double luby(double y, int x){

    // Find the finite subsequence that contains index 'x', and the
    // size of that subsequence:
    int size, seq;
    for (size = 1, seq = 0; size < x+1; seq++, size = 2*size+1);

    while (size-1 != x){
        size = (size-1)>>1;
        seq--;
        x = x % size;
    }

    return pow(y, seq);
}

// NOTE: assumptions passed in member-variable 'assumptions'.
lbool Solver::solve_()
{
    model.clear();
    conflict.clear();
    if (!ok) return l_False;

    solves++;

    max_learnts = nClauses() * learntsize_factor;
    if (max_learnts < min_learnts_lim)
        max_learnts = min_learnts_lim;

    learntsize_adjust_confl   = learntsize_adjust_start_confl;
    learntsize_adjust_cnt     = (int)learntsize_adjust_confl;
    lbool   status            = l_Undef;
    ext_rejected              = false;

    if (verbosity >= 1){
        printf("============================[ Search Statistics ]==============================\n");
        printf("| Conflicts |          ORIGINAL         |          LEARNT          | Progress |\n");
        printf("|           |    Vars  Clauses Literals |    Limit  Clauses Lit/Cl |          |\n");
        printf("===============================================================================\n");
    }

    // Search:
    int curr_restarts = 0;
    while (status == l_Undef && !ext_rejected){
        double rest_base = luby_restart ? luby(restart_inc, curr_restarts) : pow(restart_inc, curr_restarts);
        status = search(rest_base * restart_first);
        if (!withinBudget()) break;
        curr_restarts++;
    }

    if (verbosity >= 1)
        printf("===============================================================================\n");


    if (status == l_True){
        // Extend & copy model:
        model.growTo(nVars());
        for (int i = 0; i < nVars(); i++) model[i] = value(i);
    }else if (status == l_False && conflict.size() == 0)
        ok = false;

    cancelUntil(0);
    return status;
}


bool Solver::implies(const vec<Lit>& assumps, vec<Lit>& out)
{
    trail_lim.push(trail.size());
    for (int i = 0; i < assumps.size(); i++){
        Lit a = assumps[i];

        if (value(a) == l_False){
            cancelUntil(0);
            return false;
        }else if (value(a) == l_Undef)
            uncheckedEnqueue(a);
    }

    unsigned trail_before = trail.size();
    bool     ret          = true;
    if (propagate() == CRef_Undef){
        out.clear();
        for (int j = trail_before; j < trail.size(); j++)
            out.push(trail[j]);
    }else
        ret = false;
    
    cancelUntil(0);
    return ret;
}

//=================================================================================================
// External propagation:
//
// The propagator never sees the inside of BCP: propagate() is untouched, and the solver speaks to
// the propagator only once BCP has reached a fixpoint (propagateExternal), when it is about to
// decide (decideExternal), when every variable is assigned (checkModelExternal), and when
// conflict analysis needs the reason for a literal the propagator implied (explain). Assignments
// are reported in batches, from a cursor into the trail, so each costs one test of 'observed'.
// With no propagator connected, each of those places costs a NULL test, and nothing else.


void Solver::requireExternal(bool cond, const char* what) const
{
    if (cond) return;
    fprintf(stderr, "minisat: external propagator: %s\n", what);
    abort();
}


void Solver::connect_external_propagator(ExternalPropagator* p)
{
    requireExternal(p != NULL, "connect_external_propagator() needs a propagator");
    requireExternal(propagator == NULL, "connect_external_propagator() while one is connected");
    cancelUntil(0);
    propagator     = p;
    // Root assignments of variables it goes on to observe are reported too:
    notified       = 0;
    notified_level = 0;
    notify_fixed.clear();
}


void Solver::disconnect_external_propagator()
{
    if (propagator == NULL) return;
    reset_observed_vars();
    propagator = NULL;
}


void Solver::reset_observed_vars()
{
    requireExternal(!in_explanation, "reset_observed_vars() while a reason is being read");
    // Lazy reasons can only mention observed variables, and all of them are above the root:
    cancelUntil(0);
    for (int i = 0; i < observed_vars.size(); i++)
        observed[observed_vars[i]] = 0;
    observed_vars.clear();
    notify_fixed.clear();
}


void Solver::add_observed_var(Var v)
{
    requireExternal(propagator != NULL, "add_observed_var() without a connected propagator");
    requireExternal(!in_explanation, "add_observed_var() while a reason is being read");
    requireExternal(v >= 0 && v < nVars(), "add_observed_var() of a variable that does not exist");
    if (observed[v]) return;
    observed[v] = 1;
    observed_vars.push(v);

    if (value(v) == l_Undef || propagator->is_lazy) return;

    // Already assigned. If the cursor has not reached it, it is reported in its place:
    int lvl   = level(v);
    int begin = lvl == 0 ? 0 : trail_lim[lvl - 1];
    int end   = lvl == decisionLevel() ? trail.size() : trail_lim[lvl];
    if (notified <= begin) return;
    for (int i = notified; i < end; i++)
        if (var(trail[i]) == v) return;

    // Otherwise the propagator has missed it, and it has to hear of it on the level it belongs
    // to. Above the root, the assignment is undone, to be made and reported again; at the root,
    // it stands for good, and is reported as soon as the search is back there:
    if (lvl > 0)
        cancelUntil(lvl - 1);
    else{
        cancelUntil(0);
        notify_fixed.push(mkLit(v, value(v) == l_False));
    }
}


void Solver::remove_observed_var(Var v)
{
    requireExternal(!in_explanation, "remove_observed_var() while a reason is being read");
    if (!observed[v]) return;

    // An implication the propagator has yet to explain may rest on 'v'. Undo it, and everything
    // after it, before the propagator stops answering for 'v':
    if (value(v) != l_Undef && level(v) > 0)
        cancelUntil(level(v) - 1);

    observed[v] = 0;
    for (int i = 0; i < observed_vars.size(); i++)
        if (observed_vars[i] == v){
            observed_vars[i] = observed_vars.last();
            observed_vars.pop();
            break; }
    for (int i = 0; i < notify_fixed.size(); i++)
        if (var(notify_fixed[i]) == v){
            notify_fixed[i] = notify_fixed.last();
            notify_fixed.pop();
            break; }
}


bool Solver::is_decision(Lit p) const
{
    Var v = var(p);
    return value(v) != l_Undef && level(v) > 0 && reason(v) == CRef_Undef;
}


void Solver::force_backtrack(int level)
{
    requireExternal(backtrack_allowed, "force_backtrack() outside cb_decide() and cb_check_found_model()");
    requireExternal(level >= 0 && level < decisionLevel(), "force_backtrack() to a level that is not below the current one");
    cancelUntil(level);
}


void Solver::notifyAssignments_()
{
    assert(propagatorActive());
    // Root assignments observed late come first; the search is at the root while they wait:
    assert(notify_fixed.size() == 0 || decisionLevel() == 0);
    ext_lits.clear();
    for (int i = 0; i < notify_fixed.size(); i++)
        ext_lits.push(notify_fixed[i]);
    notify_fixed.clear();

    for (; notified < trail.size(); notified++)
        if (observed[var(trail[notified])])
            ext_lits.push(trail[notified]);

    if (ext_lits.size() > 0){
        ext_notifications++;
        propagator->notify_assignment(ext_lits);
    }
}


void Solver::readExternalClause(vec<Lit>& out)
{
    out.clear();
    for (Lit q = propagator->cb_add_external_clause_lit(); q != lit_Undef; q = propagator->cb_add_external_clause_lit()){
        requireExternal(var(q) >= 0 && var(q) < nVars() && observed[var(q)],
                        "an external clause has a literal that is not observed");
        out.push(q);
    }
    ext_clauses++;
}


void Solver::readReason(Lit p, vec<Lit>& out)
{
    out.clear();
    bool has_p = false;
    for (Lit q = propagator->cb_add_reason_clause_lit(p); q != lit_Undef; q = propagator->cb_add_reason_clause_lit(p)){
        requireExternal(var(q) >= 0 && var(q) < nVars() && observed[var(q)],
                        "a reason clause has a literal that is not observed");
        has_p |= q == p;
        out.push(q);
    }
    requireExternal(has_p, "a reason clause lacks the literal it explains");
    ext_reasons++;
}


/*_________________________________________________________________________________________________
|
|  addExternalClause : (ps : vec<Lit>&) (learnt : bool)  ->  [CRef]
|
|  Description:
|    Integrate a clause the propagator hands over in the middle of the search. Only the root
|    assignment simplifies it; whatever it is above the root -- satisfied, unit, or falsified on
|    a lower level than the current one -- it is attached, and acted on:
|
|      * falsified, with two literals on its highest level: the search backtracks to that level,
|        and the clause is returned, as the conflict to analyze;
|      * falsified, with one literal on its highest level: it would have implied that literal on
|        the level below, so the search backtracks there and implies it;
|      * one literal unassigned and the rest false: that literal is implied, on this level;
|      * one literal, under the root: it is asserted at the root.
|
|    Sets 'ok' to false if the clause is falsified at the root. 'ps' is changed.
|________________________________________________________________________________________________@*/
CRef Solver::addExternalClause(vec<Lit>& ps, bool learnt)
{
    sort(ps);
    Lit p; int i, j;
    for (i = j = 0, p = lit_Undef; i < ps.size(); i++){
        bool root = value(ps[i]) != l_Undef && level(var(ps[i])) == 0;
        if ((root && value(ps[i]) == l_True) || ps[i] == ~p)
            return CRef_Undef;
        else if (!(root && value(ps[i]) == l_False) && ps[i] != p)
            ps[j++] = p = ps[i];
    }
    ps.shrink(i - j);

    if (ps.size() == 0){
        ok = false;
        return CRef_Undef; }

    if (ps.size() == 1){
        cancelUntil(0);
        uncheckedEnqueue(ps[0]);
        return CRef_Undef; }

    // Watch the best two literals: true or unassigned ones, then false ones, latest first.
    for (int k = 0; k < 2; k++){
        int best = k;
        for (int m = k + 1; m < ps.size(); m++){
            int rank_m    = value(ps[m])    == l_False ? level(var(ps[m]))    : INT32_MAX;
            int rank_best = value(ps[best]) == l_False ? level(var(ps[best])) : INT32_MAX;
            if (rank_m > rank_best) best = m;
        }
        Lit tmp = ps[k]; ps[k] = ps[best]; ps[best] = tmp;
    }

    CRef cr = ca.alloc(ps, learnt);
    if (learnt){
        learnts.push(cr);
        claBumpActivity(ca[cr]);
    }else
        clauses.push(cr);
    attachClause(cr);

    Lit first = ps[0], second = ps[1];
    if (value(first) == l_True || value(second) != l_False)
        return CRef_Undef;

    if (value(first) == l_Undef){
        uncheckedEnqueue(first, cr);
        return CRef_Undef; }

    int first_level = level(var(first)), second_level = level(var(second));
    if (first_level > second_level){
        cancelUntil(second_level);
        uncheckedEnqueue(first, cr);
        return CRef_Undef; }

    ext_conflicts++;
    cancelUntil(first_level);
    return cr;
}


/*_________________________________________________________________________________________________
|
|  propagateExternal : [void]  ->  [CRef]
|
|  Description:
|    Called at a BCP fixpoint. Report the new assignments, take the propagator's implications,
|    propagating each before asking for the next, then take its clauses; repeat while it has
|    clauses to give, since those may enable more implications. Returns a conflicting clause, on
|    the level the search is now at, or CRef_Undef. Sets 'ok' to false if the propagator has
|    made the formula unsatisfiable at the root.
|________________________________________________________________________________________________@*/
CRef Solver::propagateExternal()
{
    for (;;){
        notifyAssignments();

        for (Lit p = propagator->cb_propagate(); p != lit_Undef; p = propagator->cb_propagate()){
            requireExternal(var(p) >= 0 && var(p) < nVars() && observed[var(p)],
                            "cb_propagate() returned a literal that is not observed");
            ext_propagations++;
            if (value(p) == l_True)
                continue;

            if (value(p) == l_False){
                // Implied, yet false: its reason is falsified, and is a conflict.
                readReason(p, ext_clause);
                CRef confl = addExternalClause(ext_clause, propagator->are_reasons_forgettable);
                if (!ok || confl != CRef_Undef) return confl;
            }else if (decisionLevel() == 0){
                // Nothing would ever ask for a root implication's reason. Read it now, while it
                // can still be checked, and keep the literal as a fact:
                readReason(p, ext_clause);
                for (int k = 0; k < ext_clause.size(); k++)
                    requireExternal(ext_clause[k] == p || value(ext_clause[k]) == l_False,
                                    "a reason clause has a literal that is not false");
                uncheckedEnqueue(p);
            }else
                uncheckedEnqueue(p, CRef_Lazy);

            CRef confl = propagate();
            if (confl != CRef_Undef) return confl;
            notifyAssignments();
        }

        bool added = false, forgettable = false;
        while (propagator->cb_has_external_clause(forgettable)){
            readExternalClause(ext_clause);
            added = true;
            CRef confl = addExternalClause(ext_clause, forgettable);
            if (!ok || confl != CRef_Undef) return confl;
            confl = propagate();
            if (confl != CRef_Undef) return confl;
            notifyAssignments();
            forgettable = false;
        }
        if (!added)
            return CRef_Undef;
    }
}


/*_________________________________________________________________________________________________
|
|  explain : (x : Var)  ->  [CRef]
|
|  Description:
|    Ask the propagator for the reason it implied 'x', and make it the clause that 'reason(x)'
|    refers to: the true literal of 'x' first, the latest other literal second, where
|    attachClause() wants the watches. Every other literal must be false, and assigned no later
|    than 'x' -- on its level at the latest, since trail positions are not recorded. Duplicates
|    and literals false at the root are dropped.
|
|    'x' keeps the level it was implied on, which may be above the one its reason gives it:
|    analysis stays sound, and only backjumps less far than it might.
|________________________________________________________________________________________________@*/
CRef Solver::explain(Var x)
{
    assert(reason(x) == CRef_Lazy);
    Lit p = mkLit(x, value(x) == l_False);

    in_explanation = true;
    readReason(p, ext_clause);
    in_explanation = false;

    vec<Lit>& c = ext_clause;
    sort(c);
    Lit prev; int i, j;
    for (i = j = 0, prev = lit_Undef; i < c.size(); i++){
        Lit q = c[i];
        if (q == prev || q == p) continue;
        prev = q;
        requireExternal(var(q) != x && value(q) == l_False && level(var(q)) <= level(x),
                        "a reason clause has a literal that is not false before the one it explains");
        if (level(var(q)) > 0)
            c[j++] = q;
    }
    c.shrink(i - j);

    c.push(p);
    Lit tmp = c[0]; c[0] = p; c[c.size() - 1] = tmp;
    for (int k = 2; k < c.size(); k++)
        if (level(var(c[k])) > level(var(c[1]))){
            tmp = c[1]; c[1] = c[k]; c[k] = tmp; }

    if (c.size() == 1){
        // The theory holds 'p' outright, yet it stands on the level it was implied on. Analysis
        // reads a one-literal reason, which resolves nothing away; it is never attached, and is
        // collected once 'p' is unassigned.
        CRef cr = ca.alloc(c, true);
        vardata[x].reason = cr;
        return cr;
    }

    CRef cr = ca.alloc(c, propagator->are_reasons_forgettable);
    if (ca[cr].learnt()) learnts.push(cr);
    else                 clauses.push(cr);
    attachClause(cr);
    vardata[x].reason = cr;
    return cr;
}


// The propagator's decision, lit_Undef if it leaves the choice to the solver, or lit_Error if it
// backtracked or observed a variable it had missed, in which case the search must propagate
// before it decides anything; what it returned then is not used.
Lit Solver::decideExternal()
{
    notifyAssignments();
    int level_before = decisionLevel(), trail_before = trail.size();

    backtrack_allowed = true;
    Lit p = propagator->cb_decide();
    backtrack_allowed = false;

    if (decisionLevel() != level_before || trail.size() != trail_before)
        return lit_Error;
    if (p == lit_Undef)
        return lit_Undef;
    requireExternal(var(p) >= 0 && var(p) < nVars() && observed[var(p)],
                    "cb_decide() returned a literal that is not observed");
    requireExternal(value(p) == l_Undef, "cb_decide() returned a literal that is already assigned");
    ext_decisions++;
    return p;
}


Lit Solver::pickObservedLit()
{
    for (int i = 0; i < observed_vars.size(); i++)
        if (value(observed_vars[i]) == l_Undef)
            return polarityFor(observed_vars[i]);
    return lit_Undef;
}


/*_________________________________________________________________________________________________
|
|  checkModelExternal : (confl : CRef&)  ->  [lbool]
|
|  Description:
|    Every variable is assigned. Returns l_True if the propagator accepts the assignment, l_False
|    if it rejects it without a clause to say why, and l_Undef if the search has to go on: the
|    propagator backtracked, observed something new, or gave a clause that changes the trail --
|    'confl' is set if that clause is falsified, and the search is already on its level.
|
|    Clauses the assignment satisfies leave it complete, and it is judged again.
|________________________________________________________________________________________________@*/
lbool Solver::checkModelExternal(CRef& confl)
{
    confl = CRef_Undef;
    for (;;){
        if (propagatorActive())
            notifyAssignments();

        ext_lits.clear();
        for (int i = 0; i < observed_vars.size(); i++){
            Var v = observed_vars[i];
            assert(value(v) != l_Undef);
            ext_lits.push(mkLit(v, value(v) == l_False));
        }

        int level_before = decisionLevel(), trail_before = trail.size(), observed_before = observed_vars.size();
        backtrack_allowed = true;
        bool accepted = propagator->cb_check_found_model(ext_lits);
        backtrack_allowed = false;
        ext_checks++;

        if (decisionLevel() != level_before || trail.size() != trail_before || observed_vars.size() != observed_before)
            return l_Undef;
        if (accepted)
            return l_True;

        bool added = false, forgettable = false;
        while (propagator->cb_has_external_clause(forgettable)){
            readExternalClause(ext_clause);
            added = true;
            confl = addExternalClause(ext_clause, forgettable);
            if (!ok || confl != CRef_Undef || decisionLevel() != level_before || trail.size() != trail_before)
                return l_Undef;
            forgettable = false;
        }
        if (!added)
            return l_False;
    }
}


//=================================================================================================
// Writing CNF to DIMACS:
// 
// FIXME: this needs to be rewritten completely.

static Var mapVar(Var x, vec<Var>& map, Var& max)
{
    if (map.size() <= x || map[x] == -1){
        map.growTo(x+1, -1);
        map[x] = max++;
    }
    return map[x];
}


void Solver::toDimacs(FILE* f, Clause& c, vec<Var>& map, Var& max)
{
    if (satisfied(c)) return;

    for (int i = 0; i < c.size(); i++)
        if (value(c[i]) != l_False)
            fprintf(f, "%s%d ", sign(c[i]) ? "-" : "", mapVar(var(c[i]), map, max)+1);
    fprintf(f, "0\n");
}


void Solver::toDimacs(const char *file, const vec<Lit>& assumps)
{
    FILE* f = fopen(file, "wr");
    if (f == NULL)
        fprintf(stderr, "could not open file %s\n", file), exit(1);
    toDimacs(f, assumps);
    fclose(f);
}


void Solver::toDimacs(FILE* f, const vec<Lit>& assumps)
{
    // Handle case when solver is in contradictory state:
    if (!ok){
        fprintf(f, "p cnf 1 2\n1 0\n-1 0\n");
        return; }

    vec<Var> map; Var max = 0;

    // Cannot use removeClauses here because it is not safe
    // to deallocate them at this point. Could be improved.
    int cnt = 0;
    for (int i = 0; i < clauses.size(); i++)
        if (!satisfied(ca[clauses[i]]))
            cnt++;
        
    for (int i = 0; i < clauses.size(); i++)
        if (!satisfied(ca[clauses[i]])){
            Clause& c = ca[clauses[i]];
            for (int j = 0; j < c.size(); j++)
                if (value(c[j]) != l_False)
                    mapVar(var(c[j]), map, max);
        }

    // Assumptions are added as unit clauses:
    cnt += assumps.size();

    fprintf(f, "p cnf %d %d\n", max, cnt);

    for (int i = 0; i < assumps.size(); i++){
        assert(value(assumps[i]) != l_False);
        fprintf(f, "%s%d 0\n", sign(assumps[i]) ? "-" : "", mapVar(var(assumps[i]), map, max)+1);
    }

    for (int i = 0; i < clauses.size(); i++)
        toDimacs(f, ca[clauses[i]], map, max);

    if (verbosity > 0)
        printf("Wrote DIMACS with %d variables and %d clauses.\n", max, cnt);
}


void Solver::printStats() const
{
    double cpu_time = cpuTime();
    double mem_used = memUsedPeak();
    printf("restarts              : %" PRIu64 "\n", starts);
    printf("conflicts             : %-12" PRIu64 "   (%.0f /sec)\n", conflicts   , conflicts   /cpu_time);
    printf("decisions             : %-12" PRIu64 "   (%4.2f %% random) (%.0f /sec)\n", decisions, (float)rnd_decisions*100 / (float)decisions, decisions   /cpu_time);
    printf("propagations          : %-12" PRIu64 "   (%.0f /sec)\n", propagations, propagations/cpu_time);
    printf("conflict literals     : %-12" PRIu64 "   (%4.2f %% deleted)\n", tot_literals, (max_literals - tot_literals)*100 / (double)max_literals);
    if (ext_notifications + ext_propagations + ext_clauses + ext_decisions + ext_checks > 0){
        printf("external notifications: %-12" PRIu64 "\n", ext_notifications);
        printf("external propagations : %-12" PRIu64 "   (%" PRIu64 " reasons read)\n", ext_propagations, ext_reasons);
        printf("external clauses      : %-12" PRIu64 "   (%" PRIu64 " conflicts)\n", ext_clauses, ext_conflicts);
        printf("external decisions    : %-12" PRIu64 "\n", ext_decisions);
        printf("external model checks : %-12" PRIu64 "\n", ext_checks);
    }
    if (mem_used != 0) printf("Memory used           : %.2f MB\n", mem_used);
    printf("CPU time              : %g s\n", cpu_time);
}


//=================================================================================================
// Garbage Collection methods:

void Solver::relocAll(ClauseAllocator& to)
{
    // All watchers:
    //
    watches.cleanAll();
    for (int v = 0; v < nVars(); v++)
        for (int s = 0; s < 2; s++){
            Lit p = mkLit(v, s);
            vec<Watcher>& ws = watches[p];
            for (int j = 0; j < ws.size(); j++)
                ca.reloc(ws[j].cref, to);
        }

    // All reasons:
    //
    for (int i = 0; i < trail.size(); i++){
        Var v = var(trail[i]);

        // Note: it is not safe to call 'locked()' on a relocated clause. This is why we keep
        // 'dangling' reasons here. It is safe and does not hurt.
        if (reason(v) != CRef_Undef && reason(v) != CRef_Lazy && (ca[reason(v)].reloced() || locked(ca[reason(v)]))){
            assert(!isRemoved(reason(v)));
            ca.reloc(vardata[v].reason, to);
        }
    }

    // All learnt:
    //
    int i, j;
    for (i = j = 0; i < learnts.size(); i++)
        if (!isRemoved(learnts[i])){
            ca.reloc(learnts[i], to);
            learnts[j++] = learnts[i];
        }
    learnts.shrink(i - j);

    // All original:
    //
    for (i = j = 0; i < clauses.size(); i++)
        if (!isRemoved(clauses[i])){
            ca.reloc(clauses[i], to);
            clauses[j++] = clauses[i];
        }
    clauses.shrink(i - j);
}


void Solver::garbageCollect()
{
    // Initialize the next region to a size corresponding to the estimated utilization degree. This
    // is not precise but should avoid some unnecessary reallocations for the new region:
    ClauseAllocator to(ca.size() - ca.wasted()); 

    relocAll(to);
    if (verbosity >= 2)
        printf("|  Garbage collection:   %12d bytes => %12d bytes             |\n", 
               ca.size()*ClauseAllocator::Unit_Size, to.size()*ClauseAllocator::Unit_Size);
    to.moveTo(ca);
}
