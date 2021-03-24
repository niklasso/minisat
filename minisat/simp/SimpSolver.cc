/***********************************************************************************[SimpSolver.cc]
MiniSat -- Copyright (c) 2006,      Niklas Een, Niklas Sorensson
           Copyright (c) 2007-2010, Niklas Sorensson

Chanseok Oh's MiniSat Patch Series -- Copyright (c) 2015, Chanseok Oh

Maple_LCM, Based on MapleCOMSPS_DRUP -- Copyright (c) 2017, Mao Luo, Chu-Min LI, Fan Xiao: implementing a learnt clause
minimisation approach Reference: M. Luo, C.-M. Li, F. Xiao, F. Manya, and Z. L. , “An effective learnt clause
minimization approach for cdcl sat solvers,” in IJCAI-2017, 2017, pp. to–appear.

Maple_LCM_Dist, Based on Maple_LCM -- Copyright (c) 2017, Fan Xiao, Chu-Min LI, Mao Luo: using a new branching heuristic
called Distance at the beginning of search


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

#include "simp/SimpSolver.h"
#include "mtl/Sort.h"
#include "utils/Options.h"
#include "utils/System.h"

using namespace MERGESAT_NSPACE;

//=================================================================================================
// Options:


static const char *_cat = "SIMP";

static BoolOption opt_use_asymm(_cat, "asymm", "Shrink clauses by asymmetric branching.", false);
static BoolOption opt_use_rcheck(_cat, "rcheck", "Check if a clause is already implied. (costly)", false);
static BoolOption opt_use_elim(_cat, "elim", "Perform variable elimination.", true);
static IntOption opt_grow(_cat, "grow", "Allow a variable elimination step to grow by a number of clauses.", 0);
static IntOption opt_clause_lim(_cat,
                                "cl-lim",
                                "Variables are not eliminated if it produces a resolvent with a length above this "
                                "limit. -1 means no limit",
                                20,
                                IntRange(-1, INT32_MAX));
static IntOption
opt_subsumption_lim(_cat,
                    "sub-lim",
                    "Do not check if subsumption against a clause larger than this. -1 means no limit.",
                    1000,
                    IntRange(-1, INT32_MAX));
static DoubleOption opt_simp_garbage_frac(_cat,
                                          "simp-gc-frac",
                                          "The fraction of wasted memory allowed before a garbage collection is "
                                          "triggered during simplification.",
                                          0.5,
                                          DoubleRange(0, false, HUGE_VAL, false));
static Int64Option opt_max_simplify_step(_cat,
                                         "max-simp-steps",
                                         "Do not perform more simplification steps than this. -1 means no limit.",
                                         40000000000,
                                         Int64Range(-1, INT64_MAX));
static IntOption opt_max_simp_cls(_cat,
                                  "max-simp-cls",
                                  "If input has more clauses than the given number, disable simplification",
                                  INT32_MAX,
                                  IntRange(0, INT32_MAX));


//=================================================================================================
// Constructor/Destructor:


SimpSolver::SimpSolver()
  : simp_reparsed_options(updateOptions())
  , parsing(false)
  , grow(opt_grow)
  , clause_lim(opt_clause_lim)
  , subsumption_lim(opt_subsumption_lim)
  , simp_garbage_frac(opt_simp_garbage_frac)
  , max_simp_steps(opt_max_simplify_step)
  , nr_max_simp_cls(opt_max_simp_cls)
  , use_asymm(opt_use_asymm)
  , use_rcheck(opt_use_rcheck)
  , use_elim(opt_use_elim)
  , merges(0)
  , asymm_lits(0)
  , eliminated_vars(0)
  , elimorder(1)
  , use_simplification(true)
  , occurs(ClauseDeleted(ca))
  , elim_heap(ElimLt(n_occ))
  , bwdsub_assigns(0)
  , n_touched(0)
{
    vec<Lit> dummy(1, lit_Undef);
    ca.extra_clause_field = true; // NOTE: must happen before allocating the dummy clause below.
    bwdsub_tmpunit = ca.alloc(dummy);
    remove_satisfied = false;
}


SimpSolver::~SimpSolver() {}


Var SimpSolver::newVar(bool sign, bool dvar)
{
    Var v = Solver::newVar(sign, dvar);

    frozen.push((char)false);
    eliminated.push((char)false);

    if (use_simplification) {
        n_occ.push(0);
        n_occ.push(0);
        occurs.init(v);
        touched.push(0);
        elim_heap.insert(v);
    }

    // TODO: make sure to add new data structures also to reserveVars below!

    return v;
}

void SimpSolver::reserveVars(Var v)
{
    Solver::reserveVars(v);

    frozen.capacity(v + 1);
    eliminated.capacity(v + 1);

    if (use_simplification) {
        n_occ.capacity(v + 1);
        n_occ.capacity(v + 1);
        occurs.init(v);
        touched.capacity(v + 1);
    }
}


int SimpSolver::max_simp_cls() { return nr_max_simp_cls; }

lbool SimpSolver::solve_(bool do_simp, bool turn_off_simp)
{
    vec<Var> extra_frozen;
    lbool result = l_True;

    systematic_branching_state = 1;

    do_simp &= use_simplification;
    double simp_time = cpuTime();

    const int pre_simplification_units = trail.size();

    if (do_simp) {
        // Assumptions must be temporarily frozen to run variable elimination:
        for (int i = 0; i < assumptions.size(); i++) {
            Var v = var(assumptions[i]);

            // If an assumption has been eliminated, remember it.
            assert(!isEliminated(v));

            if (!frozen[v]) {
                // Freeze and store.
                setFrozen(v, true);
                extra_frozen.push(v);
            }
        }

        result = lbool(eliminate(turn_off_simp));
    }
    occurs.clear(true);
    touched.clear(true);
    occurs.clear(true);
    n_occ.clear(true);
    elim_heap.clear(true);
    subsumption_queue.clear(true);

    simp_time = cpuTime() - simp_time;      // stop timer and record time consumed until now
    check_satisfiability_simplified = true; // only check SAT answer after the call to extendModel()

    /* share units during initial call, only really useful in case preprocessing is used */
    if (solves == 1) {
        assert(decisionLevel() == 0);
        add_tmp.clear();
        add_tmp.push(lit_Undef);
        for (int i = pre_simplification_units; i < trail.size(); ++i) {
            add_tmp[0] = trail[i];
            shareViaCallback(add_tmp, 1);
        }
        add_tmp.clear();
    }

    if (result == l_True)
        result = Solver::solve_();
    else if (verbosity >= 1)
        printf("c ===============================================================================\n");

    simp_time = cpuTime() - simp_time; // continue timer and consider time tracked already

    if (result == l_True) {
        extendModel();
        if (check_satisfiability) {
            if (!satChecker.checkModel(model)) {
                assert(false && "model should satisfy full input formula");
                throw("ERROR: detected model that does not satisfy input formula, abort");
                exit(1);
            } else if (verbosity)
                printf("c validated SAT answer after extending model\n");
        }
    }

    if (do_simp)
        // Unfreeze the assumptions that were frozen:
        for (int i = 0; i < extra_frozen.size(); i++) setFrozen(extra_frozen[i], false);

    systematic_branching_state = 0;
    statistics.simpSeconds += cpuTime() - simp_time; // stop timer and record time consumed until now

    return result;
}


bool SimpSolver::addClause_(vec<Lit> &ps)
{
#ifndef NDEBUG
    bool is_sat = false;
    bool has_eliminated = false;
    for (int i = 0; i < ps.size(); i++) {
        if (value(ps[i]) == l_True) is_sat = true;
        if (isEliminated(var(ps[i]))) has_eliminated = true;
    }
    assert((is_sat || !has_eliminated) && "removing clauses is done lazily");
#endif

    int nclauses = clauses.size();

    if (use_rcheck && implied(ps)) return true;

    if (!parsing && drup_file) {
#ifdef BIN_DRUP
        binDRUP('a', ps, drup_file);
#else
        for (int i = 0; i < ps.size(); i++) fprintf(drup_file, "%i ", (var(ps[i]) + 1) * (-2 * sign(ps[i]) + 1));
        fprintf(drup_file, "0\n");
#endif
    }

    if (!Solver::addClause_(ps)) return false;

    // Only simplify before actually solving
    if (use_simplification && clauses.size() == nclauses + 1 && solves == 0) {
        CRef cr = clauses.last();
        const Clause &c = ca[cr];
        statistics.simpSteps++;

        // NOTE: the clause is added to the queue immediately and then
        // again during 'gatherTouchedClauses()'. If nothing happens
        // in between, it will only be checked once. Otherwise, it may
        // be checked twice unnecessarily. This is an unfortunate
        // consequence of how backward subsumption is used to mimic
        // forward subsumption.
        subsumption_queue.insert(cr);
        for (int i = 0; i < c.size(); i++) {
            occurs[var(c[i])].push(cr);
            n_occ[toInt(c[i])]++;
            touched[var(c[i])] = 1;
            n_touched++;
            if (elim_heap.inHeap(var(c[i]))) elim_heap.increase(var(c[i]));
        }
    }

    return true;
}


void SimpSolver::removeClause(CRef cr)
{
    const Clause &c = ca[cr];
    statistics.simpSteps++;

    if (use_simplification)
        for (int i = 0; i < c.size(); i++) {
            n_occ[toInt(c[i])]--;
            updateElimHeap(var(c[i]));
            occurs.smudge(var(c[i]));
        }

    Solver::removeClause(cr);
}


bool SimpSolver::strengthenClause(CRef cr, Lit l)
{
    Clause &c = ca[cr];
    statistics.simpSteps++;
    assert(decisionLevel() == 0);
    assert(use_simplification);

    // FIX: this is too inefficient but would be nice to have (properly implemented)
    // if (!find(subsumption_queue, &c))
    subsumption_queue.insert(cr);

    if (drup_file) {
#ifdef BIN_DRUP
        binDRUP_strengthen(c, l, drup_file);
#else
        for (int i = 0; i < c.size(); i++)
            if (c[i] != l) fprintf(drup_file, "%i ", (var(c[i]) + 1) * (-2 * sign(c[i]) + 1));
        fprintf(drup_file, "0\n");
#endif
    }

    if (c.size() == 2) {
        removeClause(cr);
        c.strengthen(l);
    } else {
        if (drup_file) {
#ifdef BIN_DRUP
            binDRUP('d', c, drup_file);
#else
            fprintf(drup_file, "d ");
            for (int i = 0; i < c.size(); i++) fprintf(drup_file, "%i ", (var(c[i]) + 1) * (-2 * sign(c[i]) + 1));
            fprintf(drup_file, "0\n");
#endif
        }

        detachClause(cr, true);
        c.strengthen(l);
        attachClause(cr);
        remove(occurs[var(l)], cr);
        n_occ[toInt(l)]--;
        updateElimHeap(var(l));
    }

    return c.size() == 1 ? enqueue(c[0]) && propagate() == CRef_Undef : true;
}


// Returns FALSE if clause is always satisfied ('out_clause' should not be used).
bool SimpSolver::merge(const Clause &_ps, const Clause &_qs, Var v, vec<Lit> &out_clause)
{
    merges++;
    out_clause.clear();
    counter++;

    for (int i = 0; i < _ps.size(); i++) {
        if (var(_ps[i]) != v) {
            out_clause.push(_ps[i]);
            seen2[_ps[i].x] = counter;
        }
    }

    for (int i = 0; i < _qs.size(); i++) {
        if (var(_qs[i]) != v) {
            if (seen2[_qs[i].x] != counter) {
                if (seen2[(~_qs[i]).x] != counter) {
                    out_clause.push(_qs[i]);
                } else {
                    return false;
                }
            }
        }
    }

    return true;
}


// Returns FALSE if clause is always satisfied.
bool SimpSolver::merge(const Clause &_ps, const Clause &_qs, Var v, int &size)
{
    merges++;
    merge_count_cls.clear();
    bool ret = merge(_ps, _qs, v, merge_count_cls);
    size = merge_count_cls.size();
    return ret;
}


void SimpSolver::gatherTouchedClauses()
{
    if (n_touched == 0) return;

    int i, j;
    for (i = j = 0; i < subsumption_queue.size(); i++)
        if (ca[subsumption_queue[i]].mark() == 0) ca[subsumption_queue[i]].mark(2);
    statistics.simpSteps += subsumption_queue.size();

    for (i = 0; i < touched.size(); i++)
        if (touched[i]) {
            const vec<CRef> &cs = occurs.lookup(i);
            for (j = 0; j < cs.size(); j++)
                if (ca[cs[j]].mark() == 0) {
                    subsumption_queue.insert(cs[j]);
                    ca[cs[j]].mark(2);
                }
            touched[i] = 0;
            statistics.simpSteps += cs.size();
        }

    for (i = 0; i < subsumption_queue.size(); i++)
        if (ca[subsumption_queue[i]].mark() == 2) ca[subsumption_queue[i]].mark(0);
    statistics.simpSteps += subsumption_queue.size();

    n_touched = 0;
}


bool SimpSolver::implied(const vec<Lit> &c)
{
    assert(decisionLevel() == 0);

    trail_lim.push(trail.size());
    for (int i = 0; i < c.size(); i++)
        if (value(c[i]) == l_True) {
            cancelUntil(0);
            return true;
        } else if (value(c[i]) != l_False) {
            assert(value(c[i]) == l_Undef);
            uncheckedEnqueue(~c[i], decisionLevel());
        }

    bool result = propagate() != CRef_Undef;
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

    while (subsumption_queue.size() > 0 || bwdsub_assigns < trail.size()) {

        // Empty subsumption queue and return immediately on user-interrupt:
        if (asynch_interrupt || !isInSimpLimit()) {
            subsumption_queue.clear();
            bwdsub_assigns = trail.size();
            break;
        }

        // Check top-level assignments by creating a dummy clause and placing it in the queue:
        if (subsumption_queue.size() == 0 && bwdsub_assigns < trail.size()) {
            Lit l = trail[bwdsub_assigns++];
            ca[bwdsub_tmpunit][0] = l;
            ca[bwdsub_tmpunit].calcAbstraction();
            subsumption_queue.insert(bwdsub_tmpunit);
            statistics.simpSteps++;
        }

        CRef cr = subsumption_queue.peek();
        subsumption_queue.pop();
        Clause &c = ca[cr];
        statistics.simpSteps++;

        if (c.mark()) continue;

        c.setOnQueue(false);

        if (verbose && verbosity >= 2 && cnt++ % 1000 == 0)
            printf("c subsumption left: %10d (%10d subsumed, %10d deleted literals)\r", subsumption_queue.size(),
                   subsumed, deleted_literals);

        assert(c.size() > 1 || value(c[0]) == l_True); // Unit-clauses should have been propagated before this point.

        // Find best variable to scan:
        Var best = var(c[0]);
        for (int i = 1; i < c.size(); i++)
            if (occurs[var(c[i])].size() < occurs[best].size()) best = var(c[i]);

        // Search all candidates:
        vec<CRef> &_cs = occurs.lookup(best);
        CRef *cs = (CRef *)_cs;

        for (int j = 0; j < _cs.size(); j++)
            if (c.mark())
                break;
            else if (statistics.simpSteps++ && !ca[cs[j]].mark() && cs[j] != cr &&
                     (subsumption_lim == -1 || ca[cs[j]].size() < subsumption_lim)) {
                Lit l = c.subsumes(ca[cs[j]]);

                if (l == lit_Undef)
                    subsumed++, removeClause(cs[j]);
                else if (l != lit_Error) {
                    deleted_literals++;

                    if (!strengthenClause(cs[j], ~l)) return false;

                    // Did current candidate get deleted from cs? Then check candidate at index j again:
                    if (var(l) == best) j--;
                }
            }
    }

    return true;
}


bool SimpSolver::asymm(Var v, CRef cr)
{
    Clause &c = ca[cr];
    assert(decisionLevel() == 0);
    statistics.simpSteps++;

    if (c.mark() || satisfied(c)) return true;

    trail_lim.push(trail.size());
    Lit l = lit_Undef;
    for (int i = 0; i < c.size(); i++)
        if (var(c[i]) != v) {
            if (value(c[i]) != l_False) uncheckedEnqueue(~c[i], 0);
        } else
            l = c[i];

    if (propagate() != CRef_Undef) {
        cancelUntil(0);
        asymm_lits++;
        if (!strengthenClause(cr, l)) return false;
    } else
        cancelUntil(0);

    return true;
}


bool SimpSolver::asymmVar(Var v)
{
    assert(use_simplification);

    const vec<CRef> &cls = occurs.lookup(v);

    if (value(v) != l_Undef || cls.size() == 0) return true;

    for (int i = 0; i < cls.size(); i++)
        if (!asymm(v, cls[i])) return false;

    return backwardSubsumptionCheck();
}


static void mkElimClause(vec<uint32_t> &elimclauses, Lit x)
{
    elimclauses.push(toInt(x));
    elimclauses.push(1);
}


static void mkElimClause(vec<uint32_t> &elimclauses, Var v, Clause &c)
{
    int first = elimclauses.size();
    int v_pos = -1;

    // Copy clause to elimclauses-vector. Remember position where the
    // variable 'v' occurs:
    for (int i = 0; i < c.size(); i++) {
        elimclauses.push(toInt(c[i]));
        if (var(c[i]) == v) v_pos = i + first;
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
    const vec<CRef> &cls = occurs.lookup(v);
    vec<CRef> pos, neg;
    for (int i = 0; i < cls.size(); i++) (find(ca[cls[i]], mkLit(v)) ? pos : neg).push(cls[i]);
    statistics.simpSteps += cls.size();

    // Check wether the increase in number of clauses stays within the allowed ('grow'). Moreover, no
    // clause must exceed the limit on the maximal clause size (if it is set):
    //
    int cnt = 0;
    int clause_size = 0;

    for (int i = 0; i < pos.size(); i++) {
        statistics.simpSteps += neg.size();
        for (int j = 0; j < neg.size(); j++)
            if (merge(ca[pos[i]], ca[neg[j]], v, clause_size) &&
                (++cnt > cls.size() + grow || (clause_lim != -1 && clause_size > clause_lim)))
                return true;
    }

    // Delete and store old clauses:
    eliminated[v] = true;
    setDecisionVar(v, false);
    eliminated_vars++;

    if (pos.size() > neg.size()) {
        for (int i = 0; i < neg.size(); i++) mkElimClause(elimclauses, v, ca[neg[i]]);
        mkElimClause(elimclauses, mkLit(v));
        statistics.simpSteps += neg.size();
    } else {
        for (int i = 0; i < pos.size(); i++) mkElimClause(elimclauses, v, ca[pos[i]]);
        mkElimClause(elimclauses, ~mkLit(v));
        statistics.simpSteps += pos.size();
    }

    // Produce clauses in cross product:
    vec<Lit> &resolvent = add_tmp;
    for (int i = 0; i < pos.size(); i++) {
        statistics.simpSteps += neg.size();
        for (int j = 0; j < neg.size(); j++)
            if (merge(ca[pos[i]], ca[neg[j]], v, resolvent) && !addClause_(resolvent)) return false;
    }

    for (int i = 0; i < cls.size(); i++) removeClause(cls[i]);
    statistics.simpSteps += cls.size();

    // Free occurs list for this variable:
    occurs[v].clear(true);

    // Free watchers lists for this variable, if possible:
    watches_bin[mkLit(v)].clear(true);
    watches_bin[~mkLit(v)].clear(true);
    watches[mkLit(v)].clear(true);
    watches[~mkLit(v)].clear(true);

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
    const vec<CRef> &cls = occurs.lookup(v);

    vec<Lit> &subst_clause = add_tmp;
    for (int i = 0; i < cls.size(); i++) {
        Clause &c = ca[cls[i]];
        statistics.simpSteps++;

        subst_clause.clear();
        for (int j = 0; j < c.size(); j++) {
            Lit p = c[j];
            subst_clause.push(var(p) == v ? x ^ sign(p) : p);
        }

        if (!addClause_(subst_clause)) return ok = false;

        removeClause(cls[i]);
    }

    return true;
}


void SimpSolver::extendModel()
{
    int i, j;
    Lit x;

    for (i = elimclauses.size() - 1; i > 0; i -= j) {
        for (j = elimclauses[i--]; j > 1; j--, i--)
            if (modelValue(toLit(elimclauses[i])) != l_False) goto next;

        x = toLit(elimclauses[i]);
        model[var(x)] = lbool(!sign(x));
    next:;
    }
}

// Almost duplicate of Solver::removeSatisfied. Didn't want to make the base method 'virtual'.
void SimpSolver::removeSatisfied()
{
    int i, j;
    for (i = j = 0; i < clauses.size(); i++) {
        const Clause &c = ca[clauses[i]];
        if (c.mark() == 0) {
            if (satisfied(c)) {
                removeSatisfiedClause(clauses[i]);
            } else {
                clauses[j++] = clauses[i];
            }
        }
    }
    clauses.shrink(i - j);
}

// The technique and code are by the courtesy of the GlueMiniSat team. Thank you!
// It helps solving certain types of huge problems tremendously.
bool SimpSolver::eliminate(bool turn_off_elim)
{
    bool res = true;
    int iter = 0;
    int n_cls, n_cls_init, n_vars;

    systematic_branching_state = 1;

    if (nVars() == 0) goto cleanup; // User disabling preprocessing.

    // Get an initial number of clauses (more accurately).
    if (trail.size() != 0) removeSatisfied();
    n_cls_init = nClauses();

    res = eliminate_(); // The first, usual variable elimination of MiniSat.
    if (!res) goto cleanup;

    n_cls = nClauses();
    n_vars = nFreeVars();

    if (verbosity >= 1) printf("c Reduced to %d vars, %d cls (grow=%d)\n", n_vars, n_cls, grow);

    if ((double)n_cls / n_vars >= 10 || n_vars < 10000) {
        if (verbosity > 0)
            printf("c No iterative elimination performed. (vars=%d, c/v ratio=%.1f)\n", n_vars, (double)n_cls / n_vars);
        goto cleanup;
    }

    grow = grow ? grow * 2 : 8;
    for (; grow < 10000; grow *= 2) {
        // Rebuild elimination variable heap.
        for (int i = 0; i < clauses.size(); i++) {
            const Clause &c = ca[clauses[i]];
            for (int j = 0; j < c.size(); j++)
                if (!elim_heap.inHeap(var(c[j])))
                    elim_heap.insert(var(c[j]));
                else
                    elim_heap.update(var(c[j]));
        }

        int n_cls_last = nClauses();
        int n_vars_last = nFreeVars();

        res = eliminate_();
        if (!res || n_vars_last == nFreeVars()) break;
        iter++;

        int n_cls_now = nClauses();
        int n_vars_now = nFreeVars();

        double cl_inc_rate = (double)n_cls_now / n_cls_last;
        double var_dec_rate = (double)n_vars_last / n_vars_now;

        if (verbosity >= 1) {
            printf("c Reduced to %d vars, %d cls (grow=%d)\n", n_vars_now, n_cls_now, grow);
            printf("c cl_inc_rate=%.3f, var_dec_rate=%.3f\n", cl_inc_rate, var_dec_rate);
        }

        if (n_cls_now > n_cls_init || cl_inc_rate > var_dec_rate) break;
    }
    if (verbosity >= 1) printf("c No. effective iterative eliminations: %d\n", iter);

cleanup:
    touched.clear(true);
    occurs.clear(true);
    n_occ.clear(true);
    elim_heap.clear(true);
    subsumption_queue.clear(true);

    use_simplification = false;
    remove_satisfied = true;
    ca.extra_clause_field = false;

    // Force full cleanup (this is safe and desirable since it only happens once):
    rebuildOrderHeap();
    garbageCollect();

    systematic_branching_state = 0;
    return res;
}


bool SimpSolver::eliminate_()
{
    double simp_time = cpuTime();
    if (!simplify())
        return false;
    else if (!use_simplification)
        return true;

    int trail_size_last = trail.size();

    // Main simplification loop:
    //
    while (n_touched > 0 || bwdsub_assigns < trail.size() || elim_heap.size() > 0) {

        if (!isInSimpLimit()) break;

        gatherTouchedClauses();
        // printf("  ## (time = %6.2f s) BWD-SUB: queue = %d, trail = %d\n", cpuTime(), subsumption_queue.size(), trail.size() - bwdsub_assigns);
        if ((subsumption_queue.size() > 0 || bwdsub_assigns < trail.size()) && !backwardSubsumptionCheck(true)) {
            ok = false;
            goto cleanup;
        }

        // Empty elim_heap and return immediately on user-interrupt:
        if (asynch_interrupt || !isInSimpLimit()) {
            assert(bwdsub_assigns == trail.size());
            assert(subsumption_queue.size() == 0);
            assert(n_touched == 0);
            elim_heap.clear();
            goto cleanup;
        }

        // printf("  ## (time = %6.2f s) ELIM: vars = %d\n", cpuTime(), elim_heap.size());
        for (int cnt = 0; !elim_heap.empty(); cnt++) {
            Var elim = elim_heap.removeMin();

            if (asynch_interrupt || !isInSimpLimit()) break;

            if (isEliminated(elim) || value(elim) != l_Undef) continue;

            if (verbosity >= 2 && cnt % 100 == 0) printf("c elimination left: %10d\r", elim_heap.size());

            if (use_asymm) {
                // Temporarily freeze variable. Otherwise, it would immediately end up on the queue again:
                bool was_frozen = frozen[elim];
                frozen[elim] = true;
                if (!asymmVar(elim)) {
                    ok = false;
                    goto cleanup;
                }
                frozen[elim] = was_frozen;
            }

            // At this point, the variable may have been set by assymetric branching, so check it
            // again. Also, don't eliminate frozen variables:
            if (use_elim && value(elim) == l_Undef && !frozen[elim] && !eliminateVar(elim)) {
                ok = false;
                goto cleanup;
            }

            checkGarbage(simp_garbage_frac);
        }

        assert(subsumption_queue.size() == 0);
    }
cleanup:
    // To get an accurate number of clauses.
    if (trail_size_last != trail.size())
        removeSatisfied();
    else {
        int i, j;
        for (i = j = 0; i < clauses.size(); i++)
            if (ca[clauses[i]].mark() == 0) clauses[j++] = clauses[i];
        clauses.shrink(i - j);
    }
    checkGarbage();

    if (verbosity >= 1 && elimclauses.size() > 0)
        printf("c |  Eliminated clauses:     %10.2f Mb                                      |\n",
               double(elimclauses.size() * sizeof(uint32_t)) / (1024 * 1024));

    statistics.simpSeconds += cpuTime() - simp_time;

    return ok;
}


//=================================================================================================
// Garbage Collection methods:


void SimpSolver::relocAll(ClauseAllocator &to)
{
    if (!use_simplification) return;

    // All occurs lists:
    //
    occurs.cleanAll();
    if (occurs.size() >= nVars()) {
        for (int i = 0; i < nVars(); i++) {
            vec<CRef> &cs = occurs[i];
            assert((solves == 0 || cs.size() == 0) && "There should be no occurrences during solving");
            for (int j = 0; j < cs.size(); j++) ca.reloc(cs[j], to);
            statistics.simpSteps += cs.size();
        }
    }

    // Subsumption queue:
    //
    assert((solves == 0 || subsumption_queue.size() == 0) &&
           "There should be no occurrences subsumption candidates during solving");
    for (int i = subsumption_queue.size(); i > 0; i--) {
        CRef cr = subsumption_queue.peek();
        subsumption_queue.pop();
        statistics.simpSteps++;
        if (ca[cr].mark()) continue;
        ca.reloc(cr, to);
        subsumption_queue.insert(cr);
    }

    // Temporary clause:
    //
    ca.reloc(bwdsub_tmpunit, to);
}


void SimpSolver::garbageCollect()
{
    // Initialize the next region to a size corresponding to the estimated utilization degree. This
    // is not precise but should avoid some unnecessary reallocations for the new region:
    ClauseAllocator to(ca.size() - ca.wasted());

    to.extra_clause_field = ca.extra_clause_field; // NOTE: this is important to keep (or lose) the extra fields.
    relocAll(to);
    Solver::relocAll(to);
    if (verbosity >= 2)
        printf("c |  Garbage collection:   %12d bytes => %12d bytes             |\n",
               ca.size() * ClauseAllocator::Unit_Size, to.size() * ClauseAllocator::Unit_Size);
    to.moveTo(ca);
}

Lit SimpSolver::subsumes(Clause &c1, Clause &c2)
{

    Lit ret = lit_Undef;
    if (c1.size() > c2.size() || (c1.abstraction() & ~c2.abstraction()) != 0) {
        return lit_Error;
    }

    counter++;

    for (int i = 0; i < c2.size(); i++) seen2[c2[i].x] = counter;
    for (int i = 0; i < c1.size(); i++) {
        if (seen2[c1[i].x] != counter) {
            if (ret == lit_Undef && seen2[(~c1[i]).x] == counter)
                ret = c1[i];
            else
                ret = lit_Error;
        }
    }
    return ret;
}

void SimpSolver::addLearnedClause(const vec<Lit> &cls)
{
    /* do not receive clauses that contain eliminated variables */
    for (int i = 0; i < cls.size(); ++i)
        if (isEliminated(var(cls[i]))) return;
    Solver::addLearnedClause(cls);
}

void SimpSolver::diversify(int rank, int size)
{
    /* rank ranges from 0 to size-1 */

    /* keep first 2 configurations as is,
       and disable simplification for last 2 configurations */
    if (rank > 1 && rank >= size - 2) use_simplification = false;

    /* allow higher grow value for last 2 configurations with simplfication */
    if (rank > 1 && rank >= size - 4) grow = 8;

    /* have a configuration allowed to simplify more on longer clauses */
    if (rank > 1 && rank >= size - 5) clause_lim = 40;

    Solver::diversify(rank, size);

    /* in case we shall not*/
    if (!use_simplification) eliminate(true);
}