/***********************************************************************************[sat-simple.cc]
Copyright (c) 2021, Norbert Manthey

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

#include "tests/TestSolver.h"

using namespace MERGESAT_NSPACE;

bool TestSolver::test_entrypoint()
{
    /* Make sure the values are as expected after solving */
    test_assert(okay(), "solver has to be okay");

    test_assert(value(mkLit(1)) == l_True, "first variable has to be true");
    test_assert(value(mkLit(2)) == l_True, "second variable has to be true");
    test_assert(value(mkLit(3)) == l_True, "third variable has to be true");

    return true;
}

int main(int argc, char **argv)
{
    TestSolver solver;

    solver.verbosity = 0;

    while (solver.nVars() < 3) solver.newVar();

    solver.addClause(mkLit(1));
    solver.addClause(mkLit(2));
    solver.addClause(mkLit(3));

    bool status = solver.solve();

    test_assert(status == true, "The given formula has to be satisfiable");

    return 0;
}
