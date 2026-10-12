/*****************************************************************************[ExternalPropagatorFuzz.cc]
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

// A randomised differential test of the external propagator interface.
//
// Each case draws a random CNF and splits it. The solver under test is given the "base" clauses;
// the "theory" clauses are known only to a propagator, which enforces them through the interface
// in every way the interface allows -- implying literals and explaining them when asked, handing
// over conflicts at once or late, handing over clauses that are unit or satisfied, implying
// literals that are already false, deciding, advising polarity, forcing backtracks, observing new
// variables in the middle of a search, and judging models. Its view of the assignment is built
// from notifications alone, and is checked against the solver at every callback. Answers are
// checked against a plain solver given every clause, as are models and final conflicts, over
// several incremental solves; odd seeds run Solver, even ones SimpSolver.
//
//   ExternalPropagatorFuzz [cases] [first-seed]

#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <random>
#include <vector>

#include "minisat/core/Solver.h"
#include "minisat/simp/SimpSolver.h"

using namespace Minisat;

typedef std::vector<Lit> Lits;

static unsigned current_seed;
static bool     trace = getenv("FUZZ_TRACE") != NULL;

static void check(bool cond, const char* what)
{
    if (cond) return;
    fprintf(stderr, "FAIL (seed %u): %s\n", current_seed, what);
    exit(1);
}

// Exposes what the propagator's checks need to see of the solver under test.
template<class S>
struct Exposed : public S {
    using S::decisionLevel;
};

// SimpSolver eliminates variables in its first solve; anything that may be observed, or used in
// a later clause or assumption, has to survive it.
template<class S> static void freeze(Exposed<S>&, Var) {}
template<> void freeze<SimpSolver>(Exposed<SimpSolver>& s, Var v) { s.setFrozen(v, true); }
template<class S> static bool simplifying() { return false; }
template<> bool simplifying<SimpSolver>() { return true; }

// How a propagator goes about its work. Each case draws one.
struct Style {
    bool lazy;               // Judge complete assignments only.
    bool propagate;          // Imply unit literals through cb_propagate.
    bool forgettable;        // Reasons may be forgotten.
    bool advise;             // Advise decision polarity.
    int  delay;              // Per mille: hold back a conflict for another poll.
    int  extra;              // Per mille: hand over a theory clause that is not yet a conflict.
    int  false_implication;  // Per mille: imply a literal that is already false.
    int  decide;             // Per mille: make a decision.
    int  backtrack;          // Per mille: force a backtrack.
    int  observe_late;       // Per mille: observe a further variable in the middle of a search.
    int  duplicate;          // Per mille: repeat a literal in a reason or a clause.
};

template<class S>
class TheoryPropagator : public ExternalPropagator {
    Exposed<S>&         s;
    std::mt19937&       rng;
    Style               style;
    std::vector<Lits>&  theory;

    std::vector<lbool>  view;       // Per variable, from notifications only.
    std::vector<Lit>    trail;      // Notified literals, in order.
    std::vector<int>    lim;        // Where each notified level starts in 'trail'.
    std::vector<char>   watched;    // Observed, as far as this propagator knows.
    std::vector<Var>    later;      // Variables it may observe in the middle of a search.
    std::vector<Lits>   reasons;    // Per literal index: the clause it was last implied by.
    std::vector<char>   offered;    // Per theory clause: handed over as an extra already.
    Lits                pending;    // The clause a rejected model violated.
    int                 held;       // Polls a conflict has been held back for.
    int                 backtracks; // Forced so far. Bounded: a search undone often enough
                                    // never reaches a model, and that is the test's doing.

    Lits                out;        // Clause or reason being handed over.
    size_t              out_at;
    bool                out_busy;
    Lit                 reason_for;

public:
    TheoryPropagator(Exposed<S>& s, std::mt19937& rng, const Style& st, std::vector<Lits>& theory)
        : s(s), rng(rng), style(st), theory(theory), held(0), backtracks(0), out_at(0), out_busy(false), reason_for(lit_Undef)
    {
        is_lazy                 = st.lazy;
        are_reasons_forgettable = st.forgettable;
        advises_polarity        = st.advise;
        view.assign(s.nVars(), l_Undef);
        watched.assign(s.nVars(), 0);
        reasons.assign(2 * s.nVars(), Lits());
        theoryGrew();
    }

    void theoryGrew() { offered.assign(theory.size(), 0); }

    bool chance(int per_mille) { return (int)(rng() % 1000) < per_mille; }

    void observe(Var v)      { watched[v] = 1; s.add_observed_var(v); }
    void observeLater(Var v) { later.push_back(v); }

    lbool valueOf(Lit p) const { return view[var(p)] ^ sign(p); }
    bool  observed(const Lits& c) const {
        for (size_t k = 0; k < c.size(); k++) if (!watched[var(c[k])]) return false;
        return true; }

    // The view agrees with the solver about the level and every observed variable.
    void crossCheck() {
        if (is_lazy) return;
        check((int)lim.size() == s.decisionLevel(), "notified levels differ from the solver's");
        for (Var v = 0; v < (Var)view.size(); v++)
            if (watched[v])
                check(view[v] == s.value(v), "notified assignment differs from the solver's");
    }

    void notify_assignment(const vec<Lit>& lits) {
        check(!is_lazy, "a lazy propagator was notified");
        for (int i = 0; i < lits.size(); i++){
            Lit p = lits[i];
            check(watched[var(p)], "notified of a variable that is not observed");
            check(view[var(p)] == l_Undef, "notified twice of one assignment");
            check(s.value(p) == l_True, "notified of a literal that is not true");
            view[var(p)] = lbool(!sign(p));
            trail.push_back(p);
        }
    }

    void notify_new_decision_level() {
        check(!is_lazy, "a lazy propagator was notified");
        lim.push_back((int)trail.size());
    }

    void notify_backtrack(int level) {
        check(!is_lazy, "a lazy propagator was notified");
        check(level >= 0 && level < (int)lim.size(), "backtrack to a level that was not open");
        while ((int)trail.size() > lim[level]){
            view[var(trail.back())] = l_Undef;
            trail.pop_back();
        }
        lim.resize(level);
    }

    void handOver(const Lits& c) {
        out = c;
        if (chance(style.duplicate) && out.size() > 0)
            out.push_back(out[rng() % out.size()]);
        std::shuffle(out.begin(), out.end(), rng);
        out_at   = 0;
        out_busy = true;
    }

    Lit next() {
        if (out_at == out.size()){
            out_busy = false;
            return lit_Undef; }
        return out[out_at++];
    }

    Lit cb_propagate() {
        crossCheck();
        if (!style.propagate) return lit_Undef;
        for (size_t i = 0; i < theory.size(); i++){
            const Lits& c = theory[i];
            if (!observed(c)) continue;
            int open = -1, open_count = 0, true_count = 0;
            for (size_t k = 0; k < c.size(); k++){
                lbool v = valueOf(c[k]);
                if      (v == l_Undef) { open = (int)k; open_count++; }
                else if (v == l_True)  true_count++;
            }
            if (true_count > 0) continue;
            if (open_count == 1){
                reasons[toInt(c[open])] = c;
                return c[open];
            }
            if (open_count == 0 && chance(style.false_implication)){
                Lit p = c[rng() % c.size()];
                reasons[toInt(p)] = c;
                return p;
            }
        }
        return lit_Undef;
    }

    Lit cb_add_reason_clause_lit(Lit p) {
        if (!out_busy || reason_for != p){
            check(!out_busy, "a reason was asked for in the middle of another clause");
            check(reasons[toInt(p)].size() > 0, "asked for the reason of a literal never implied");
            handOver(reasons[toInt(p)]);
            reason_for = p;
        }
        Lit q = next();
        if (q == lit_Undef) reason_for = lit_Undef;
        return q;
    }

    bool cb_has_external_clause(bool& forgettable) {
        crossCheck();
        check(!out_busy, "a clause was announced while another was being read");
        if (!pending.empty()){
            handOver(pending);
            pending.clear();
            forgettable = chance(500);
            return true;
        }
        if (is_lazy) return false;

        // A conflict, unless it is held back for a while:
        for (size_t i = 0; i < theory.size(); i++){
            const Lits& c = theory[i];
            if (!observed(c)) continue;
            bool all_false = true;
            for (size_t k = 0; k < c.size() && all_false; k++)
                all_false = valueOf(c[k]) == l_False;
            if (!all_false) continue;
            if (held < 3 && chance(style.delay)){
                held++;
                break; }
            held = 0;
            forgettable = chance(500);
            handOver(c);
            return true;
        }

        // A clause that is no conflict yet, once each:
        if (chance(style.extra) && theory.size() > 0){
            size_t i = rng() % theory.size();
            if (!offered[i] && observed(theory[i])){
                offered[i] = 1;
                forgettable = chance(500);
                handOver(theory[i]);
                return true;
            }
        }
        return false;
    }

    Lit cb_add_external_clause_lit() {
        check(out_busy && reason_for == lit_Undef, "a clause literal was read with no clause announced");
        return next();
    }

    bool observeLate() {
        if (later.empty() || !chance(style.observe_late)) return false;
        Var v = later.back();
        later.pop_back();
        observe(v);
        return true;
    }

    bool cb_check_found_model(const vec<Lit>& model) {
        crossCheck();
        int observed_count = 0;
        for (size_t v = 0; v < watched.size(); v++) observed_count += watched[v];
        check(model.size() == observed_count, "the model does not cover the observed variables");
        std::vector<lbool> m(s.nVars(), l_Undef);
        for (int i = 0; i < model.size(); i++){
            check(watched[var(model[i])], "the model has a variable that is not observed");
            check(s.value(model[i]) == l_True, "the model has a literal that is not true");
            check(is_lazy || valueOf(model[i]) == l_True, "the model differs from the notifications");
            m[var(model[i])] = lbool(!sign(model[i]));
        }

        // Either of these sends the search on, whatever is returned:
        if (s.decisionLevel() > 0 && backtracks < 50 && chance(style.backtrack)){
            backtracks++;
            s.force_backtrack(rng() % s.decisionLevel());
            return false; }
        if (observeLate())
            return false;

        for (size_t i = 0; i < theory.size(); i++){
            check(observed(theory[i]), "a theory clause has a variable that is not observed");
            bool sat = false;
            for (size_t k = 0; k < theory[i].size(); k++)
                sat |= (m[var(theory[i][k])] ^ sign(theory[i][k])) == l_True;
            if (!sat){
                pending = theory[i];
                return false; }
        }
        return true;
    }

    Lit cb_decide() {
        crossCheck();
        if (s.decisionLevel() > 0 && backtracks < 50 && chance(style.backtrack)){
            backtracks++;
            s.force_backtrack(rng() % s.decisionLevel());
            return lit_Undef; }
        if (observeLate())
            return lit_Undef;
        if (!chance(style.decide)) return lit_Undef;
        std::vector<Var> open;
        for (Var v = 0; v < (Var)view.size(); v++)
            if (watched[v] && s.value(v) == l_Undef) open.push_back(v);
        if (open.empty()) return lit_Undef;
        return mkLit(open[rng() % open.size()], rng() & 1);
    }

    Lit cb_decide_polarity(Lit p) {
        check(watched[var(p)] && s.value(p) == l_Undef, "polarity asked for a variable that is not open and observed");
        return (rng() & 1) ? ~p : p;
    }
};

// Mostly ternary, so that the clause/variable ratios below straddle the threshold; a few units,
// binaries and long clauses besides.
static Lits randomClause(std::mt19937& rng, const std::vector<Var>& vars, bool ternary = false)
{
    int r   = rng() % 100;
    int len = ternary ? 3 : r < 2 ? 1 : r < 17 ? 2 : r < 87 ? 3 : 4 + rng() % 6;
    Lits c;
    for (int i = 0; i < len; i++)
        c.push_back(mkLit(vars[rng() % vars.size()], rng() & 1));
    return c;
}

static void toVec(const Lits& c, vec<Lit>& out) { out.clear(); for (size_t k = 0; k < c.size(); k++) out.push(c[k]); }

static bool satisfies(const vec<lbool>& model, const Lits& c)
{
    for (size_t i = 0; i < c.size(); i++)
        if ((model[var(c[i])] ^ sign(c[i])) == l_True)
            return true;
    return false;
}

// The answer a plain solver gives for all the clauses under the assumptions.
static lbool reference(int vars, const std::vector<Lits>& base, const std::vector<Lits>& theory, const vec<Lit>& assumps)
{
    Solver r;
    vec<Lit> c;
    for (int v = 0; v < vars; v++) r.newVar();
    for (size_t i = 0; i < base.size(); i++)  { toVec(base[i], c);   r.addClause(c); }
    for (size_t i = 0; i < theory.size(); i++){ toVec(theory[i], c); r.addClause(c); }
    return r.solve(assumps) ? l_True : l_False;
}

static uint64_t totals[8];

template<class S>
static void runCase(std::mt19937& rng, int& sat_count, int& unsat_count)
{
    // One case in ten is random 3-SAT at the threshold, hard enough for restarts, and run with
    // limits low enough for clause database reductions and garbage collections to happen while
    // lazy reasons are on the trail.
    bool big         = rng() % 10 == 0;
    int vars         = big ? 90 + rng() % 50 : 4 + rng() % 40;
    int clauses      = (int)(vars * (big ? 4.1 + (rng() % 30) / 100.0 : 2.5 + (rng() % 250) / 100.0));
    int theory_share = 100 + rng() % 800;    // per mille

    Style st;
    st.lazy              = rng() % 6 == 0;
    st.propagate         = rng() % 3 != 0;
    st.forgettable       = rng() & 1;
    st.advise            = rng() % 3 == 0;
    st.delay             = rng() % 3 == 0 ? 300 : 0;
    st.extra             = rng() % 3 == 0 ? 100 : 0;
    st.false_implication = rng() % 3 == 0 ? 200 : 0;
    st.decide            = rng() % 3 == 0 ? 300 : 0;
    st.backtrack         = rng() % 4 == 0 ? 30 : 0;
    st.observe_late      = rng() % 3 == 0 ? 200 : 0;
    st.duplicate         = rng() % 3 == 0 ? 200 : 0;
    bool connect_late    = rng() % 3 == 0;   // Connect after a first solve, as STP does.

    Exposed<S> s;
    std::vector<Var> all, kept;
    for (int v = 0; v < vars; v++) all.push_back(s.newVar());
    if (big){
        s.learntsize_factor = 0.02;
        s.garbage_frac      = 0.02;
    }

    // Each variable is theory, observed anyway (now or late), or neither. Under SimpSolver, those
    // of the third kind are fair game for elimination, and stay out of later clauses and
    // assumptions.
    enum { Free, Now, Late };
    std::vector<int> plan(vars, Free);
    std::vector<Lits> base, theory;
    for (int i = 0; i < clauses; i++){
        Lits c = randomClause(rng, all, big);
        if ((int)(rng() % 1000) < theory_share){
            theory.push_back(c);
            for (size_t k = 0; k < c.size(); k++) plan[var(c[k])] = Now;
        }else
            base.push_back(c);
    }
    for (Var v = 0; v < vars; v++){
        if (plan[v] == Free && rng() % 4 == 0)
            plan[v] = st.observe_late && (rng() & 1) ? Late : Now;
        if (plan[v] != Free || !simplifying<S>()){
            kept.push_back(v);
            freeze(s, v);
        }
    }
    if (kept.empty()){ kept.push_back(0); freeze(s, 0); }

    vec<Lit> c;
    for (size_t i = 0; i < base.size(); i++){ toVec(base[i], c); s.addClause(c); }

    TheoryPropagator<S>* prop = NULL;
    std::vector<Lits>   none;
    auto connect = [&](){
        prop = new TheoryPropagator<S>(s, rng, st, theory);
        s.connect_external_propagator(prop);
        for (Var v = 0; v < vars; v++)
            if      (plan[v] == Now)  prop->observe(v);
            else if (plan[v] == Late) prop->observeLater(v);
    };
    if (!connect_late) connect();

    int rounds = 1 + rng() % 4;
    if (trace)
        fprintf(stderr, "seed %u: %s, %d vars, %zu base, %zu theory, lazy %d propagate %d delay %d extra %d false %d decide %d backtrack %d late %d dup %d advise %d connect_late %d, %d rounds\n",
                current_seed, simplifying<S>() ? "SimpSolver" : "Solver", vars, base.size(), theory.size(), st.lazy, st.propagate, st.delay, st.extra,
                st.false_implication, st.decide, st.backtrack, st.observe_late, st.duplicate, st.advise, connect_late, rounds);
    for (int round = 0; round < rounds && s.okay(); round++){
        if (connect_late && round == 1) connect();
        const std::vector<Lits>& active = prop ? theory : none;

        vec<Lit> assumps;
        int n_assumps = rng() % 4 == 0 ? 0 : rng() % 4;
        for (int i = 0; i < n_assumps; i++) assumps.push(mkLit(kept[rng() % kept.size()], rng() & 1));

        lbool got  = s.solveLimited(assumps);
        check(got != l_Undef, "the solve gave up");
        lbool want = reference(vars, base, active, assumps);
        if (got != want)
            fprintf(stderr, "round %d: got %s, want %s\n", round, got == l_True ? "sat" : "unsat", want == l_True ? "sat" : "unsat");
        check(got == want, "wrong answer");

        if (got == l_True){
            sat_count++;
            for (size_t i = 0; i < base.size(); i++)   check(satisfies(s.model, base[i]),   "the model falsifies a base clause");
            for (size_t i = 0; i < active.size(); i++) check(satisfies(s.model, active[i]), "the model falsifies a theory clause");
            for (int i = 0; i < assumps.size(); i++)   check(s.modelValue(assumps[i]) == l_True, "the model falsifies an assumption");
        }else{
            unsat_count++;
            // The final conflict is negated assumptions, and refutes on its own.
            vec<Lit> core;
            for (int i = 0; i < s.conflict.size(); i++){
                Lit q = s.conflict[i];
                bool negated_assumption = false;
                for (int k = 0; k < assumps.size(); k++) negated_assumption |= assumps[k] == ~q;
                check(negated_assumption, "the final conflict has a literal that is not a negated assumption");
                core.push(~q);
            }
            check(reference(vars, base, active, core) == l_False, "the final conflict does not refute");
        }

        // Between solves: another base clause, more theory, a fresh propagator, or nothing.
        int what = rng() % 5;
        if (what == 0){
            Lits nc = randomClause(rng, kept);
            base.push_back(nc);
            toVec(nc, c);
            s.addClause(c);
        }else if (what == 1 && prop){
            Lits nc = randomClause(rng, kept);
            bool all_observed = true;
            for (size_t k = 0; k < nc.size(); k++) all_observed &= s.is_observed(var(nc[k]));
            if (all_observed){
                theory.push_back(nc);
                prop->theoryGrew(); }
        }else if (what == 2 && prop){
            s.disconnect_external_propagator();
            delete prop;
            connect();
        }
    }
    if (prop){
        s.disconnect_external_propagator();
        delete prop;
    }
    totals[0] += s.ext_notifications; totals[1] += s.ext_propagations; totals[2] += s.ext_reasons;
    totals[3] += s.ext_clauses;       totals[4] += s.ext_conflicts;    totals[5] += s.ext_decisions;
    totals[6] += s.ext_checks;        totals[7] += s.conflicts;
}

int main(int argc, char** argv)
{
    int      cases = argc > 1 ? atoi(argv[1]) : 2000;
    unsigned first = argc > 2 ? (unsigned)atoi(argv[2]) : 1;
    int sat = 0, unsat = 0;
    for (int i = 0; i < cases; i++){
        current_seed = first + (unsigned)i;
        std::mt19937 rng(current_seed);
        if (current_seed % 2) runCase<Solver>(rng, sat, unsat);
        else                  runCase<SimpSolver>(rng, sat, unsat);
    }
    printf("%d cases, %d sat and %d unsat answers checked\n", cases, sat, unsat);
    printf("notifications %llu, propagations %llu, reasons %llu, clauses %llu, external conflicts %llu, decisions %llu, model checks %llu, conflicts %llu\n",
           (unsigned long long)totals[0], (unsigned long long)totals[1], (unsigned long long)totals[2], (unsigned long long)totals[3],
           (unsigned long long)totals[4], (unsigned long long)totals[5], (unsigned long long)totals[6], (unsigned long long)totals[7]);
    return 0;
}
