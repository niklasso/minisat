/************************************************************************************[SimpSolver.h]
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

#ifndef MergeSat_TestSolver_h
#define MergeSat_TestSolver_h

#include "simp/SimpSolver.h"

namespace MERGESAT_NSPACE
{

//=================================================================================================

void test_assert(bool condition, const char *description)
{
    /* Allow to stop here during debugging */
    assert(condition);

    if (!condition) {
        printf("c condition %s fails, abort\n", (description == NULL ? "<unknown>" : description));
        _exit(1);
    }
}

/* Provide a class to access solver internals easily */
class TestSolver : public SimpSolver
{
    public:
    // Constructor/Destructor:
    //
    TestSolver();
    ~TestSolver();

    /* This function has to be implemented by the test itself */
    bool test_entrypoint();
};

inline TestSolver::TestSolver() : SimpSolver() {}
inline TestSolver::~TestSolver() {}

//=================================================================================================
} // namespace MERGESAT_NSPACE

#endif
