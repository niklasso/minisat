/**************************************************************************************[IntTypes.h]
Copyright (c) 2009,      Niklas Sorensson

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

#ifndef Minisat_IntTypes_h
#define Minisat_IntTypes_h

// NOTE: __STDC_VERSION__ >= 199901L only seems to work for compiling pure C.

#if __GNUC__
    // GCC allows use of C99-standard headers also in C++:
    
#   define __STDC_LIMIT_MACROS
#   define __STDC_FORMAT_MACROS
    
#   include <stdint.h>
#   include <inttypes.h>
    
#   undef __STDC_LIMIT_MACROS
#   undef __STDC_FORMAT_MACROS
    
    
#else
    // Workarounds for other compilers:
    
#   ifdef __SUNPRO_CC
        // Not sure if there are newer versions that support C99 headers. The
        // needed features are implemented in the headers below though:
        
#       include <sys/int_types.h>
#       include <sys/int_fmtio.h>
#       include <sys/int_limits.h>
        
#   else
        // NOTE: Add support for other compilers here.

#       error "Unsupported compiler: needs support for C99-style int-types."
        
#   endif
    
#endif

//=================================================================================================

#endif
