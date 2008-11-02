/*****************************************************************************************[Alloc.h]
Copyright (c) 2008,      Niklas Sorensson

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


#ifndef Minisat_Alloc_h
#define Minisat_Alloc_h

#include "mtl/Vec.h"

namespace Minisat {

//=================================================================================================
// Simple Region-based memory allocator:

template<class T>
class RegionAllocator
{
    vec<uint32_t>  memory;
    int            wasted;

 public:
    // TODO: make this a class for better type-checking?
    typedef uint32_t Ref;
    enum { Ref_Undef = UINT32_MAX };

    RegionAllocator() : wasted(0) { memory.capacity(1024*1024); }

    int            size       () const { return memory.size() * sizeof(T); }
    int            wastedBytes() const { return wasted * sizeof(T); }

    Ref            alloc(int size);
    void           free (int size)    { wasted += size; }

    // Deref/Load Effective Address:
    T&             drf  (Ref r)       { return *(T*)&memory[r]; }
    const T&       drf  (Ref r) const { return *(T*)&memory[r]; }
    T*             lea  (Ref r)       { return  (T*)&memory[r]; }
    const T&       lea  (Ref r) const { return  (T*)&memory[r]; }

    void           moveTo(RegionAllocator& to) { 
        memory.moveTo(to.memory); 
        to.wasted = wasted;
    }
};


template<class T>
typename RegionAllocator<T>::Ref
RegionAllocator<T>::alloc(int size)
{ 
    int end   = memory.size();
    //int cap = memory.capacity();
    memory.growTo(memory.size() + size);
    //if (cap < memory.capacity())
    //    fprintf(stderr, "new capacity: %8d (%p)\n", memory.capacity(), (char*)memory);
    return end;
}



//=================================================================================================

};

#endif
