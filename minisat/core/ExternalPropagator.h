/*****************************************************************************[ExternalPropagator.h]
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

#ifndef Minisat_ExternalPropagator_h
#define Minisat_ExternalPropagator_h

#include "minisat/mtl/Vec.h"
#include "minisat/core/SolverTypes.h"

namespace Minisat {

//=================================================================================================
// ExternalPropagator -- something outside the solver that takes part in its search.
//
// This is the IPASIR-UP interface (Fazekas, Niemetz, Preiner, Kirchweger, Szeider and Biere,
// "IPASIR-UP: User Propagators for CDCL", SAT 2023), as CaDiCaL defines it, in MiniSat's own
// types: a literal is a 'Lit', "no literal" is 'lit_Undef', and a list of literals is a
// 'vec<Lit>'. A propagator written against CaDiCaL's ExternalPropagator translates one callback
// at a time, and the contract of each is CaDiCaL's unless a comment below says otherwise.
//
// The solver tells the propagator about the variables it has asked to observe
// (Solver::add_observed_var) and nothing else: their assignments, in trail order, and every
// opening and closing of a decision level. The propagator answers with implied literals, whose
// reasons it supplies only if the solver ever asks, with clauses, which may be conflicts, and
// with a verdict on every complete assignment before the solver calls it a model.
//
// Calls into the solver from a callback:
//   * add_observed_var, remove_observed_var, is_observed and is_decision are allowed from any
//     cb_* callback except cb_add_reason_clause_lit, which runs inside conflict analysis.
//   * force_backtrack is allowed from cb_decide and cb_check_found_model only.
//   * notify_* callbacks report; they must not call back into the solver to change anything.
//   * Nothing else: no new clauses through Solver::addClause, no solve, no disconnect.
//
// Observing or unobserving a variable that is already assigned above the root backtracks, so
// that the assignment is reported -- or undone -- where it belongs; notify_backtrack arrives
// before add_observed_var returns. A callback that does this must answer for the assignment as it
// is afterwards: an implication or a decision it worked out before is likely stale.
//
// Every literal a propagator hands over -- implied, decided, in a reason or in a clause -- must
// be over an observed variable. Observed variables are frozen: SimpSolver never eliminates one.

class ExternalPropagator {
public:
    // A lazy propagator judges complete assignments only. It is not notified of anything, is
    // never asked to propagate, decide or explain, and offers clauses only after rejecting a
    // model in cb_check_found_model.
    bool is_lazy;

    // Whether clauses that explain a propagation may later be deleted by clause database
    // reduction. They are kept for as long as they are the reason for an assignment either way.
    bool are_reasons_forgettable;

    // Not in IPASIR-UP: when set, every decision the solver's own heuristic makes on an observed
    // variable is offered to cb_decide_polarity first, which may flip its sign. Off by default,
    // so that a propagator that has no opinion pays nothing for the hook.
    bool advises_polarity;

    ExternalPropagator() : is_lazy(false), are_reasons_forgettable(false), advises_polarity(false) {}
    virtual ~ExternalPropagator() {}

    // Notifications. Assignments arrive in batches, in trail order, at the latest before the
    // next callback that might depend on them; a level is opened before any assignment made on
    // it is reported. 'notify_backtrack(l)' keeps levels 0..l and undoes everything above,
    // including assignments that were made there but not yet reported.
    virtual void notify_assignment       (const vec<Lit>& lits) = 0;
    virtual void notify_new_decision_level()                    = 0;
    virtual void notify_backtrack        (int new_level)        = 0;

    // Every observed variable is assigned and no clause is falsified: return true to accept the
    // assignment as a model. 'model' holds one literal per observed variable, the one that is
    // true. Returning false rejects it, and the propagator must then offer at least one clause
    // through cb_has_external_clause; a rejection without one ends the solve with l_Undef.
    // force_backtrack, or observing an unassigned variable, also sends the search on.
    virtual bool cb_check_found_model(const vec<Lit>& model) = 0;

    // A literal to decide on next, or lit_Undef to leave it to the solver. It must be unassigned.
    virtual Lit  cb_decide() { return lit_Undef; }

    // Not in IPASIR-UP, and only consulted under 'advises_polarity': the solver's heuristic is
    // about to decide 'lit'. Return 'lit' to keep it or '~lit' to decide the other way.
    virtual Lit  cb_decide_polarity(Lit lit) { return lit; }

    // A literal implied by the current assignment, or lit_Undef if there is none. An implied
    // literal that is already true is ignored; one that is already false is a conflict, and its
    // reason is asked for at once. Otherwise the literal is assigned and its reason is asked for
    // only if conflict analysis needs it -- or at once, at decision level 0, where nothing
    // else would ever ask.
    virtual Lit  cb_propagate() { return lit_Undef; }

    // The reason for a literal earlier returned by cb_propagate, one literal per call, then
    // lit_Undef: 'propagated_lit' itself and the negations of what implies it, each of which
    // was assigned before it.
    virtual Lit  cb_add_reason_clause_lit(Lit propagated_lit) { (void)propagated_lit; return lit_Undef; }

    // Whether there is a clause to add; if so, it is read one literal per call to
    // cb_add_external_clause_lit, then lit_Undef. A clause may be anything: satisfied, unit
    // under the assignment, or falsified, in which case it is a conflict at the level it
    // belongs to. 'is_forgettable' arrives false and may be set to let clause database
    // reduction delete the clause later.
    virtual bool cb_has_external_clause    (bool& is_forgettable) = 0;
    virtual Lit  cb_add_external_clause_lit()                     = 0;
};

//=================================================================================================
}

#endif
