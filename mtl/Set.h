/*******************************************************************************************[Set.h]
MiniSat -- Copyright (c) 2006-2007, Niklas Sorensson

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

#ifndef Set_h
#define Set_h

#include <stdint.h>

#include "Vec.h"

//=================================================================================================
// Default hash/equals functions
//

template<class K> struct Hash  { uint32_t operator()(const K& k)               const { return hash(k);  } };
template<class K> struct Equal { bool     operator()(const K& k1, const K& k2) const { return k1 == k2; } };

template<class K> struct DeepHash  { uint32_t operator()(const K* k)               const { return hash(*k);  } };
template<class K> struct DeepEqual { bool     operator()(const K* k1, const K* k2) const { return *k1 == *k2; } };

//=================================================================================================
// Some primes
//

static const int nprimes          = 25;
static const int primes [nprimes] = { 31, 73, 151, 313, 643, 1291, 2593, 5233, 10501, 21013, 42073, 84181, 168451, 337219, 674701, 1349473, 2699299, 5398891, 10798093, 21596719, 43193641, 86387383, 172775299, 345550609, 691101253 };

//=================================================================================================
// Hash Set implementation:
//

template<class T, class H = Hash<T>, class E = Equal<T> >
class Set {
    H       hash;
    E       equals;

    vec<T>* table;
    int     cap;
    int     size;

    // Don't allow copying (error prone):
    Set<T,H,E>&  operator = (Set<T,H,E>& other) { assert(0); }
                 Set        (Set<T,H,E>& other) { assert(0); }

    int32_t index  (const T& e) const { return hash(e) % cap; }
    void   _insert (const T& e) { table[index(e)].push(e); }
    void    rehash () {
        const vec<T>* old = table;

        int oldsize = cap;
        cap = primes[0];
        for (int i = 1; cap <= oldsize && i < nprimes; i++)
            cap = primes[i];

        table = new vec<T>[cap];

        for (int i = 0; i < oldsize; i++){
            for (int j = 0; j < old[i].size(); j++){
                _insert(old[i][j]); }}

        delete [] old;

    }

    
    public:

     Set () : table(NULL), cap(0), size(0) {}
     Set (const H& h, const E& e) : hash(h), equals(e), table(NULL), cap(0), size(0) {}
    ~Set () { delete [] table; }

    void insert (const T& e) { if (size+1 > cap / 2) rehash(); _insert(e); size++; }
    bool peek   (T& e) {
        if (size == 0) return false;
        const vec<T>& ps = table[index(e)];
        for (int i = 0; i < ps.size(); i++)
            if (equals(ps[i], e)){
                e = ps[i];
                return true; } 
        return false;
    }

    void remove (const T& e) {
        assert(table != NULL);
        vec<T>& ps = table[index(e)];
        int j = 0;
        for (; j < ps.size() && !equals(ps[j], e); j++);
        assert(j < ps.size());
        ps[j] = ps.last();
        ps.pop();
    }

    void clear  () {
        cap = size = 0;
        delete [] table;
        table = NULL;
    }
};

#endif
