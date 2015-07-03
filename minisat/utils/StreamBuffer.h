/**********************************************************************************[StreamBuffer.h]
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

#ifndef Minisat_StreamBuffer_h
#define Minisat_StreamBuffer_h

#include <stdlib.h>
#include <stdio.h>

#include <zlib.h>

#include "minisat/mtl/XAlloc.h"

namespace Minisat {

//-------------------------------------------------------------------------------------------------
// A simple buffered character stream class:

class StreamBuffer {
    gzFile         in;
    unsigned char* buf;
    int            pos;
    int            size;

    enum { buffer_size = 64*1024 };

    void assureLookahead() {
        if (pos >= size) {
            pos  = 0;
            size = gzread(in, buf, buffer_size); } }

public:
    explicit StreamBuffer(gzFile i) : in(i), pos(0), size(0){
        buf = (unsigned char*)xrealloc(NULL, buffer_size);
        assureLookahead();
    }
    ~StreamBuffer() { free(buf); }

    int  operator *  () const { return (pos >= size) ? EOF : buf[pos]; }
    void operator ++ ()       { pos++; assureLookahead(); }
    int  position    () const { return pos; }
};

//-------------------------------------------------------------------------------------------------
// End-of-file detection functions for StreamBuffer:

static inline bool isEof(StreamBuffer& in) { return *in == EOF;  }

//=================================================================================================
}

#endif
