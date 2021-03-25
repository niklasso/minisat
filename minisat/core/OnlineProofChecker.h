/****************************************************************************[OnlineProofChecker.h]
Copyright (c) 2020, Norbert Manthey
**************************************************************************************************/

#ifndef OnlineProofChecker_h
#define OnlineProofChecker_h

#include "core/Constants.h"
#include "core/SolverTypes.h"
#include "mtl/Alg.h"
#include "mtl/Heap.h"
#include "mtl/Vec.h"
#include "utils/System.h"

#include <vector>

namespace MERGESAT_NSPACE
{

/// distinguish between DRUP and DRAT proofs
enum ProofStyle {
    unknownProof = 0,
    drupProof = 1,
    dratProof = 2,
};

#define DOUT(x) x

/** class that can check DRUP/DRAT proof on the fly during executing the SAT solver
 * Note: for DRAT clauses only the very first literal will be used for checking RAT!
 * (useful for debugging)
 */
class OnlineProofChecker
{
    public:
    protected:
    bool ok;          // indicate whether an empty clause has been in the input!
    ProofStyle proof; // current format
    ClauseAllocator ca;
    vec<CRef> clauses;
    vec<Lit> unitClauses; // store how many unit clauses have been seen already for a given literal (simply count per literal, use for propagation initialization!)
    std::vector<std::vector<CRef>> occ; // use for propagation!

    OccLists<Lit, vec<Watcher>, WatcherDeleted> watches;

    MarkArray ma; // array to mark literals (to find a clause, or for resolution)

    // two watched literal
    void attachClause(CRef cr); // Attach a clause to watcher lists.
    void detachClause(CRef cr); // Detach a clause to watcher lists.
    Var newVar();               // data structures for more variables are necessary
    int nVars() const;          // number of current variables


    // unit propagation
    int qhead;          // Head of queue (as index into the trail -- no more explicit propagation queue in MiniSat).
    vec<Lit> trail;     // Assignment stack; stores all assigments made in the order they were made.
    vec<Lit> lits;      // vector of literals for resolution
    vec<lbool> assigns; // current assignment

    lbool value(Var x) const;     // The current value of a variable.
    lbool value(Lit p) const;     // The current value of a literal.
    bool propagate();             // Perform unit propagation. Returns possibly conflicting clause.
    void uncheckedEnqueue(Lit p); // Enqueue a literal. Assumes value of literal is undefined.
    void cancelUntil();           // Backtrack until a certain level.

    vec<Lit> tmpLits; // for temporary addidion
    int verbose;

    public:
    /** setup a proof checker with the given format */
    OnlineProofChecker(ProofStyle proofStyle);

    /** add a clause of the initial formula */
    void addParsedclause(const vec<Lit> &cls);

    /** add a clause during search
     * Note: for DRAT clauses only the very first literal will be used for checking RAT!
     * @return true, if the addition of this clause is valid wrt. the current proof format
     */
    bool addClause(const std::vector<int> &clause);
    template <class T> bool addClause(const T &clause, const Lit &remLit, bool checkOnly = false);
    bool addClause(const vec<Lit> &cls, bool checkOnly = false);
    bool addClause(const Lit &l);
    template <class T> bool addStrengthenedClause(const T &clause, const Lit toDropLit, bool checkOnly = false);

    /** remove a clause during search */
    bool removeClause(const std::vector<int> &clause);
    bool removeClause(const Lit &l);
    template <class T> bool removeClause(const T &cls);
    template <class T> bool removeClause(const T &cls, const Lit &rmLit);

    /** check whether the given clause exists in the current proof (useful for debugging) */
    template <class T> bool hasClause(const T &cls);
    bool hasClause(const Lit &cls);

    /// plot the current unit clauses and the current formula
    void printState();

    /// check whether all clauses in the online checker are correctly in the data structures!
    void fullCheck();

    /// set verbosity of the checker
    void setVerbosity(int newVerbosity);
};

inline OnlineProofChecker::OnlineProofChecker(ProofStyle proofStyle)
  : ok(true), proof(proofStyle), watches(WatcherDeleted(ca)), qhead(0), verbose(10)
{
}

inline void OnlineProofChecker::setVerbosity(int newVerbosity) { verbose = newVerbosity; }

inline void OnlineProofChecker::attachClause(CRef cr)
{
    const Clause &c = ca[cr];
    assert(c.size() > 1 && "cannot watch unit clauses!");
    assert(c.mark() == 0 && "satisfied clauses should not be attached!");

    /* do not distinguish binary and longer clauses for now */
    watches[~c[0]].push(Watcher(cr, c[1]));
    watches[~c[1]].push(Watcher(cr, c[0]));
}

inline void OnlineProofChecker::detachClause(CRef cr)
{
    const Clause &c = ca[cr];
    removeUnSort(watches[~c[0]], Watcher(cr, c[1]));
    removeUnSort(watches[~c[1]], Watcher(cr, c[0]));
}

inline int OnlineProofChecker::nVars() const { return assigns.size(); }

inline Var OnlineProofChecker::newVar()
{
    int v = nVars();
    watches.init(mkLit(v, false));
    watches.init(mkLit(v, true));

    assigns.push(l_Undef);
    occ.push_back(std::vector<CRef>()); // there are no new clauses here yet
    occ.push_back(std::vector<CRef>());
    ma.resize(ma.size() + 2); // for each literal have a cell
    trail.capacity(v + 1);
    return v;
}

inline void OnlineProofChecker::uncheckedEnqueue(Lit p)
{
    DOUT(if (verbose > 3) std::cerr << "c [DRAT-OTFC] enqueue literal " << p << std::endl;);
    assigns[var(p)] = lbool(!sign(p));
    // prefetch watch lists
    __builtin_prefetch(&watches[p]);
    trail.push_(p);
}

inline void OnlineProofChecker::cancelUntil()
{
    for (int c = trail.size() - 1; c >= 0; c--) {
        assigns[var(trail[c])] = l_Undef;
    }
    qhead = 0;
    trail.clear();
}

inline lbool OnlineProofChecker::value(Var x) const { return assigns[x]; }
inline lbool OnlineProofChecker::value(Lit p) const { return assigns[var(p)] ^ sign(p); }

inline bool OnlineProofChecker::propagate()
{
    CRef confl = CRef_Undef;
    int num_props = 0;
    watches.cleanAll();

    DOUT(if (verbose > 3) std::cerr << "c [DRAT-OTFC] propagate ... " << std::endl;);

    // propagate units first!
    DOUT(if (verbose > 4) std::cerr << "c [DRAT-OTFC] propagate " << unitClauses.size() << " units" << std::endl;);
    for (int i = 0; i < unitClauses.size(); ++i) { // propagate all known units
        const Lit l = unitClauses[i];
        if (value(l) == l_True) {
            continue;
        } else if (value(l) == l_False) {
            return true;
        } else {
            uncheckedEnqueue(l);
        }
    }


    while (qhead < trail.size()) {
        Lit p = trail[qhead++]; // 'p' is enqueued fact to propagate.
        num_props++;
        DOUT(if (verbose > 5) std::cerr << "c [DRAT-OTFC] propagate lit " << p << std::endl;);

        vec<Watcher> &ws = watches[p];
        DOUT(if (verbose > 6) std::cerr << "c [DRAT-OTFC] propagate with " << ws.size() << " longer clauses" << std::endl;);
        Watcher *i, *j, *end;
        // propagate longer clauses here!
        for (i = j = (Watcher *)ws, end = i + ws.size(); i != end;) {
            DOUT(if (verbose > 7) std::cerr << "c [DRAT-OTFC] propagate with clause " << ca[i->cref] << std::endl;);

            // Try to avoid inspecting the clause:
            const Lit blocker = i->blocker;
            if (value(blocker) == l_True) { // keep binary clauses, and clauses where the blocking literal is satisfied
                DOUT(if (verbose > 8) std::cerr << "c [DRAT-OTFC] clause is true by blocker " << blocker << " in "
                                                << ca[i->cref] << std::endl;);
                *j++ = *i++;
                continue;
            }

            // Make sure the false literal is data[1]:
            const CRef cr = i->cref;
            Clause &c = ca[cr];
            const Lit false_lit = ~p;
            if (c[0] == false_lit) {
                c[0] = c[1], c[1] = false_lit;
            }
            assert(c[1] == false_lit && "wrong literal order in the clause!");
            i++;

            // If 0th watch is true, then clause is already satisfied.
            Lit first = c[0];
            const Watcher &w = Watcher(cr, first);            // updates the blocking literal
            if (first != blocker && value(first) == l_True) { // satisfied clause
                DOUT(if (verbose > 8) std::cerr << "c [DRAT-OTFC] clause is true by other watch " << first << " in "
                                                << c << std::endl;);
                *j++ = w;
                continue;
            } // same as goto NextClause;

            // Look for new watch:
            for (int k = 2; k < c.size(); k++)
                if (value(c[k]) != l_False) {
                    DOUT(if (verbose > 8) std::cerr << "c [DRAT-OTFC] found new watch for the clause: " << c[k]
                                                    << " in " << c << std::endl;);
                    c[1] = c[k];
                    c[k] = false_lit;
                    watches[~c[1]].push(w);
                    goto NextClause;
                } // no need to indicate failure of lhbr, because remaining code is skipped in this case!

            // Did not find watch -- clause is unit under assignment:
            *j++ = w;
            if (value(first) == l_False) {
                confl = cr;
                qhead = trail.size();
                // Copy the remaining watches:
                while (i < end) {
                    *j++ = *i++;
                }
                break;
            } else {
                DOUT(if (verbose > 8) std::cerr << "c [DRAT-OTFC] enqueue " << first << " with reason " << ca[cr] << std::endl;);
                uncheckedEnqueue(first);
            }
        NextClause:;
        }
        ws.shrink_(i - j); // remove all duplciate clauses!
    }
    DOUT(if (verbose > 5) std::cerr << "c [DRAT-OTFC] propagate returns " << (confl != CRef_Undef) << std::endl;);
    return (confl != CRef_Undef); // return true, if something was found!
}

inline bool OnlineProofChecker::removeClause(const std::vector<int> &clause)
{
    tmpLits.clear();
    for (size_t i = 0; i < clause.size(); ++i) {
        tmpLits.push(clause[i] < 0 ? mkLit(-clause[i] - 1, true) : mkLit(clause[i] - 1, false));
    }
    // remove this clause in the usual way
    return removeClause(tmpLits);
}

inline bool OnlineProofChecker::removeClause(const Lit &l)
{
    // create a clause with l
    tmpLits.clear();
    tmpLits.push(l);
    // add this clause in the usual way
    return removeClause(tmpLits);
}

template <class T> inline bool OnlineProofChecker::removeClause(const T &cls, const Lit &remLit)
{
    tmpLits.clear();
    if (remLit != lit_Undef) {
        tmpLits.push(remLit);
    }
    for (int i = 0; i < cls.size(); ++i) {
        if (cls[i] != remLit) {
            tmpLits.push(cls[i]);
        }
    }
    // remove this clause in the usual way
    return removeClause(tmpLits);
}

inline bool OnlineProofChecker::hasClause(const Lit &cls)
{
    vec<Lit> l;
    l.push(cls);
    return hasClause(l);
}

template <class T> inline bool OnlineProofChecker::hasClause(const T &cls)
{
    if (verbose > 3) {
        std::cerr << "c [DRAT-OTFC] check for clause " << cls << std::endl;
    }

    if (cls.size() == 0) {
        if (!ok) {
            return true;
        } else {
            return false;
        }
    }

    if (cls.size() == 1) {
        const Lit &l = cls[0];
        int i = 0;
        for (; i < unitClauses.size(); ++i) {
            if (unitClauses[i] == l) {
                return true;
            }
        }
        if (verbose > 1) {
            std::cerr << "c [DRAT-OTFC] could not find clause " << cls << std::endl;
        }
        return false;
    }
    // find correct CRef ...
    ma.nextStep();
    int smallestIndex = 0;
    ma.setCurrentStep(toInt(cls[0]));
    for (int i = 1; i < cls.size(); ++i) {
        ma.setCurrentStep(toInt(cls[i]));
        if (occ[toInt(cls[i])].size() < occ[smallestIndex].size()) {
            smallestIndex = i;
        }
    }

    const Lit smallest = cls[smallestIndex];

    CRef ref = CRef_Undef;
    size_t i = 0;
    for (; i < occ[toInt(smallest)].size(); ++i) {
        const Clause &c = ca[occ[toInt(smallest)][i]];
        if (c.size() != cls.size()) {
            continue;
        } // do not check this clause!

        int hit = 0;
        for (; hit < c.size(); ++hit) {
            if (!ma.isCurrentStep(toInt(c[hit]))) {
                break;
            } // a literal is not in both clauses, not the correct clause
        }
        if (hit < c.size()) {
            continue;
        } // not the right clause -> check next!

        ref = occ[toInt(smallest)][i];
        i = ~0UL; // set to indicate that we found the clause
        break;
    }
    if (i == occ[toInt(smallest)].size() || ref == CRef_Undef) {
        if (verbose > 1) {
            std::cerr << "c [DRAT-OTFC] could not find clause " << cls << " from list of literal " << smallest << std::endl;
        }
        printState();
        return false;
    }
    assert(i == ~0UL && "found the clause");
    return true; // found clause
}


template <class T> inline bool OnlineProofChecker::removeClause(const T &cls)
{
    if (verbose > 3) {
        std::cerr << "c [DRAT-OTFC] remove clause " << cls << std::endl;
        printState();
    }

    if (cls.size() == 0 || !ok) {
        return true;
    } // do not handle empty clauses here!
    if (cls.size() == 1) {
        const Lit l = cls[0];
        int i = 0;
        bool found = false;
        for (; i < unitClauses.size(); ++i) {
            if (unitClauses[i] == l) {
                unitClauses[i] = unitClauses[unitClauses.size() - 1];
                unitClauses.pop();
                found = true;
                break; // remove only once!
            }
        }
        if (!found) {
            assert(false && "the unit clause should be inside the vector of units");
            return false;
        }
        if (verbose > 1) {
            std::cerr << "c [DRAT-OTFC] removed clause " << cls << std::endl;
        }
        return true;
    }
    // find correct CRef ...
    ma.nextStep();
    int smallestIndex = 0;
    ma.setCurrentStep(toInt(cls[0]));
    for (int i = 1; i < cls.size(); ++i) {
        ma.setCurrentStep(toInt(cls[i]));
        if (occ[toInt(cls[i])].size() < occ[smallestIndex].size()) {
            smallestIndex = i;
        }
    }

    const Lit smallest = cls[smallestIndex];

    CRef ref = CRef_Undef;
    size_t i = 0;
    for (; i < occ[toInt(smallest)].size(); ++i) {
        const Clause &c = ca[occ[toInt(smallest)][i]];
        if (c.size() != cls.size()) {
            continue;
        } // do not check this clause!

        int hit = 0;
        for (; hit < c.size(); ++hit) {
            if (!ma.isCurrentStep(toInt(c[hit]))) {
                break;
            } // a literal is not in both clauses, not the correct clause
        }
        if (hit < c.size()) {
            continue;
        } // not the right clause -> check next!

        ref = occ[toInt(smallest)][i];
        // remove from this occ-list
        occ[toInt(smallest)][i] = occ[toInt(smallest)][occ[toInt(smallest)].size() - 1];
        occ[toInt(smallest)].pop_back();
        i = ~0; // set to indicate that we found the clause
        break;
    }
    if (i == occ[toInt(smallest)].size() || ref == CRef_Undef) {
        if (verbose > 1) {
            std::cerr << "c [DRAT-OTFC] could not remove clause " << cls << " from list of literal " << smallest << std::endl;
        }
        printState();
        assert(false && "clause should be in the data structures");
        return false;
    }

    // remove from the other occ-lists!
    for (int i = 0; i < cls.size(); ++i) {
        if (i == smallestIndex) {
            continue;
        }
        std::vector<CRef> &list = occ[toInt(cls[i])];
        size_t j = 0;
        for (; j < list.size(); ++j) {
            if (list[j] == ref) {
                list[j] = list[list.size() - 1];
                list.pop_back(); // remove the element
                j = -1;          // point somewhere invalid
                break;
            }
        }
        if (j == list.size()) {
            if (verbose > 1) {
                std::cerr << "c could not remove clause " << cls << " from list of literal " << cls[i] << std::endl;
            }
            printState();
            if (verbose > 2) {
                std::cerr << "c list for " << cls[i] << " : ";
                for (size_t k = 0; k < list.size(); ++k) {
                    std::cerr << "c " << ca[list[k]] << std::endl;
                }
            }
            assert(false && "should be able to remove the clause from all lists");
        }
    }

    // remove the clause from the two watched literal structures!
    detachClause(ref);

    // tell the garbage-collection-system that the clause can be removed
    ca[ref].mark(1);
    ca.free(ref);

    if (verbose > 1) {
        std::cerr << "c [DRAT-OTFC] removed clause " << cls << " which is internally " << ca[ref] << std::endl;
    }
    // check garbage collection once in a while!
    // TODO
    return true;
}

inline void OnlineProofChecker::addParsedclause(const vec<Lit> &cls)
{
    if (cls.size() == 0) {
        ok = false;
        return;
    }

    // have enough variables present
    for (int i = 0; i < cls.size(); ++i) {
        while (nVars() <= var(cls[i])) {
            newVar();
        }
    }

    if (cls.size() > 1) {
        CRef ref = ca.alloc(cls, false);
        for (int i = 0; i < cls.size(); ++i) {
            occ[toInt(cls[i])].push_back(ref);
        }
        attachClause(ref);
        clauses.push(ref);
    } else {
        unitClauses.push(cls[0]);
    }

    if (verbose > 1) {
        std::cerr << "c added clause " << cls << std::endl;
    }
    // here, do not check whether the clause is entailed, because its still input!
}

inline bool OnlineProofChecker::addClause(const std::vector<int> &clause)
{
    // create a clause where remLit is the first literal
    tmpLits.clear();
    for (size_t i = 0; i < clause.size(); ++i) {
        tmpLits.push(clause[i] < 0 ? mkLit(-clause[i] - 1, true) : mkLit(clause[i] - 1, false));
    }
    // add this clause in the usual way
    return addClause(tmpLits);
}

template <class T> inline bool OnlineProofChecker::addClause(const T &clause, const Lit &remLit, bool checkOnly)
{
    // create a clause where remLit is the first literal
    tmpLits.clear();
    if (remLit != lit_Undef) {
        tmpLits.push(remLit);
    }
    for (int i = 0; i < clause.size(); ++i) {
        if (clause[i] != remLit) {
            tmpLits.push(clause[i]);
        }
    }
    // add this clause in the usual way
    return addClause(tmpLits, checkOnly);
}

template <class T>
inline bool OnlineProofChecker::addStrengthenedClause(const T &clause, const Lit toDropLit, bool checkOnly)
{
    // create a clause without literal remLit
    tmpLits.clear();
    for (int i = 0; i < clause.size(); ++i) {
        if (clause[i] != toDropLit) {
            tmpLits.push(clause[i]);
        }
    }
    // add this clause in the usual way
    return addClause(tmpLits, checkOnly);
}

inline bool OnlineProofChecker::addClause(const Lit &l)
{
    // create a clause with l
    tmpLits.clear();
    if (l != lit_Undef) {
        tmpLits.push(l);
    }
    // add this clause in the usual way
    return addClause(tmpLits);
}

inline bool OnlineProofChecker::addClause(const vec<Lit> &cls, bool checkOnly)
{
    if (!ok) {
        return true;
    } // trivially true, we reached the empty clause already!
    bool conflict = false;

    const int initialVars = nVars();

    // have enough variables present
    for (int i = 0; i < cls.size(); ++i) {
        while (nVars() <= var(cls[i])) {
            newVar();
        }
    }

    if (verbose > 3) {
        std::cerr << "c [DRAT-OTFC] add/check clause " << cls << std::endl;
        printState();
    }

    // enqueue all complementary literals!
    cancelUntil();
    for (int i = 0; i < cls.size(); ++i) {
        if (value(cls[i]) == l_Undef) {
            uncheckedEnqueue(~cls[i]);
        } else if (value(~cls[i]) == l_False) {
            conflict = true;
            break;
        }
    }

    if (!conflict) {
        DOUT(if (verbose > 3) std::cerr << "c [DRAT-OTFC] clause does not conflict by pure enquing " << std::endl;);
        if (propagate()) {
            conflict = true; // DRUP!
            DOUT(if (verbose > 3) std::cerr << "c [DRAT-OTFC] clause is DRUP " << std::endl;);
        } else {
            DOUT(if (verbose > 3) std::cerr << "c [DRAT-OTFC] clause is not DRUP " << std::endl;);
            if (proof == dratProof) { // are we checking DRAT?
                if (cls.size() == 0 && checkOnly) {
                    return false;
                }
                assert(cls.size() > 0 && "checking the empty clause cannot reach here (it can only be a RUP clause, "
                                         "not a RAT clause -- empty clause is not entailed");
                if (initialVars >= var(cls[0])) { // DRAT on the first variable, because this variable is not present before!
                    // build all resolents on the first literal!
                    ma.nextStep();
                    lits.clear();
                    for (int i = 1; i < cls.size(); ++i) {
                        if (ma.isCurrentStep(toInt(~cls[i]))) {
                            conflict = true;
                            break;
                        }                                 // clause has complementary literals, accept it!
                        ma.setCurrentStep(toInt(cls[i])); // mark all except the first literal!
                        lits.push(cls[i]);
                    }
                    if (!conflict) {
                        const int initialSize =
                        lits.size(); // these literals are added by resolving with  the current clause on its first literal
                        assert(initialSize + 1 == cls.size() &&
                               "initial resolvent size has all literals except the literal to resolve on");
                        const Lit resolveLit = cls[0];
                        if (verbose > 3) {
                            std::cerr << "c [DRAT-OTFC] use literal " << resolveLit << " for DRAT check" << std::endl;
                        }
                        conflict = true;
                        const std::vector<CRef> &list = occ[toInt(~resolveLit)];
                        bool resovleConflict = false;
                        if (verbose > 4) {
                            std::cerr << "c [DRAT-OTFC] resolve against " << list.size() << " clauses" << std::endl;
                        }
                        for (size_t i = 0; i < list.size(); ++i) {
                            // build resolvent
                            lits.shrink_(lits.size() - initialSize); // remove literals from previous addClause call
                            const Clause &d = ca[list[i]];
                            if (verbose > 4) {
                                std::cerr << "c [DRAT-OTFC] resolve with clause " << d << std::endl;
                            }
                            int j = 0;
                            for (; j < d.size(); ++j) {
                                if (d[j] == ~resolveLit) {
                                    continue;
                                } // this literal is used for resolution
                                if (ma.isCurrentStep(toInt(~d[j]))) {
                                    break;
                                } // resolvent is a tautology
                                if (!ma.isCurrentStep(toInt(d[j]))) {
                                    // ma.setCurrentStep( toInt(d[j]) );  // do not do this, because there are multiple
                                    // resolvents! // this step might be redundant -- however, here we can ensure that also clauses with duplicate literals can be handled, as well as tautologic clauses!
                                    lits.push(d[j]);
                                }
                            }
                            if (j != d.size()) {
                                if (verbose > 4) {
                                    std::cerr << "c [DRAT-OTFC] resolvent is tautology" << std::endl;
                                }
                                continue; // this resolvent would be tautological!
                            }
                            // lits contains all literals of the resolvent, propagate and check for the conflict!

                            if (verbose > 3) {
                                std::cerr << "c [DRAT-OTFC] test resolvent " << lits << std::endl;
                            }
                            // enqueue all complementary literals!
                            resovleConflict = false;
                            cancelUntil();
                            for (int k = 0; k < lits.size(); ++k) {
                                if (value(lits[k]) == l_Undef) {
                                    uncheckedEnqueue(~lits[k]);
                                } else if (value(~lits[k]) == l_False) {
                                    resovleConflict = true;
                                    DOUT(if (verbose > 4) std::cerr << "c [DRAT-OTFC] the clause " << cls
                                                                    << " conflicts during enqueing its literals" << std::endl;);
                                    break;
                                } // the clause itself is tautological ...
                            }
                            if (!resovleConflict && !propagate()) {
                                conflict = false; // not DRAT, the current resolvent does not lead to a conflict!
                                if (verbose > 1) {
                                    std::cerr << "c [DRAT-OTFC] the clause " << cls
                                              << " is not a DRAT clause -- resolution on " << resolveLit << " with "
                                              << d << " failed! (does not result in a conflict with UP)" << std::endl;
                                }
                                printState();
                                assert(false && "added clause has to be a DRAT clause");
                                return false;
                            } else {
                                DOUT(if (verbose > 6) std::cerr << "c [DRAT-OTFC] the clause " << cls
                                                                << " conflicts during enqueing/propagating" << std::endl;);
                            }
                        }
                        assert(conflict && "added clause has to be a DRAT clause");
                        if (!conflict) {
                            return false;
                        }
                    } // else conflict == true
                } else {
                    conflict = true;
                } // DRAT, because we have a fresh variable!
            } else {
                if (verbose > 1) {
                    std::cerr << "c [DRAT-OTFC] the clause " << cls << " is not a DRUP clause, and we do not check DRAT"
                              << std::endl;
                }
                printState();
                assert(false && "added clause has to be a DRUP clause");
                return false;
            }
        }
    }

    // do not actually add the clause
    if (checkOnly) {
        return true;
    }

    // add the clause ...
    if (cls.size() == 0) {
        ok = false;
        return true;
    }

    if (cls.size() > 1) {
        CRef ref = ca.alloc(cls, false);
        for (int i = 0; i < cls.size(); ++i) {
            occ[toInt(cls[i])].push_back(ref);
        }
        attachClause(ref);
        clauses.push(ref);
    } else {
        unitClauses.push(cls[0]);
    }
    if (verbose > 1) {
        std::cerr << "c [DRAT-OTFC] added the clause " << cls << std::endl;
    }
    return true;
}

inline void OnlineProofChecker::printState()
{
    if (verbose < 2) {
        return;
    }

    fullCheck();

    std::cerr << "c [DRAT-OTFC] STATE:" << std::endl;
    for (int i = 0; i < unitClauses.size(); ++i) {
        std::cerr << unitClauses[i] << " 0" << std::endl;
    }
    for (int i = 0; i < clauses.size(); ++i) {
        const Clause &clause = ca[clauses[i]];
        if (clause.mark() != 0) {
            continue;
        } // jump over this clause
        for (int j = 0; j < clause.size(); ++j) {
            std::cerr << clause[j] << " ";
        }
        std::cerr << "0" << std::endl;
    }
}

inline void OnlineProofChecker::fullCheck()
{
    for (int i = 0; i < clauses.size(); ++i) {
        const CRef cr = clauses[i];
        const Clause &c = ca[cr];
        if (c.mark()) {
            continue;
        }

        if (c.size() == 1) {
            std::cerr << "there should not be unit clauses! [" << cr << "]" << c << std::endl;
        } else {
            for (int j = 0; j < 2; ++j) {
                const Lit l = ~c[j];
                vec<Watcher> &ws = watches[l];
                bool didFind = false;
                for (int j = 0; j < ws.size(); ++j) {
                    CRef wcr = ws[j].cref;
                    if (wcr == cr) {
                        didFind = true;
                        break;
                    }
                }
                if (!didFind) {
                    std::cerr << "could not find clause[" << cr << "] " << c << " in watcher for lit " << l << std::endl;
                }
            }
        }
    }

    for (Var v = 0; v < nVars(); ++v) {
        for (int p = 0; p < 2; ++p) {
            const Lit l = mkLit(v, p == 1);
            vec<Watcher> &ws = watches[l];
            for (int j = 0; j < ws.size(); ++j) {
                CRef wcr = ws[j].cref;
                const Clause &c = ca[wcr];
                if (c[0] != ~l && c[1] != ~l) {
                    std::cerr << "wrong literals for clause [" << wcr << "] " << c << " are watched. Found in list for "
                              << l << std::endl;
                }
            }
        }
    }
}

}; // namespace MERGESAT_NSPACE

#endif
