/************************************************************************************[ParseUtils.h]
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

#ifndef MergeSat_ParseUtils_h
#define MergeSat_ParseUtils_h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef USE_LIBZ
#include <zlib.h>
#endif

#include "mtl/XAlloc.h"

namespace MERGESAT_NSPACE
{

//-------------------------------------------------------------------------------------------------
// A simple buffered character stream class:

static const int buffer_size = 1048576;


class StreamBuffer
{
#ifdef USE_LIBZ
    gzFile in;
#else
    FILE *in;
#endif

    unsigned char buf[buffer_size];
    int pos;
    int size;

    void assureLookahead()
    {
        if (pos >= size) {
            pos = 0;
#ifdef USE_LIBZ
            size = gzread(in, buf, sizeof(buf));
#else
            size = fread(buf, sizeof(unsigned char), sizeof(buf), in);
#endif
        }
    }

    public:
#ifdef USE_LIBZ
    explicit StreamBuffer(gzFile i) : in(i), pos(0), size(0) { assureLookahead(); }
#else
    explicit StreamBuffer(FILE *i) : in(i), pos(0), size(0) { assureLookahead(); }
#endif

    int operator*() const { return (pos >= size) ? EOF : buf[pos]; }
    void operator++()
    {
        pos++;
        assureLookahead();
    }
    int position() const { return pos; }
};

inline bool has_suffix(const char *str, const char *suffix)
{
    size_t l = strlen(str);
    size_t k = strlen(suffix);
    if (l < k) return false;
    return !strcmp(str + l - k, suffix);
}

inline bool file_readable(const char *path)
{
    if (!path) return false;
    struct stat buf;
    if (stat(path, &buf)) return false;
    if (access(path, R_OK)) return false;
    return true;
}

inline size_t file_size(const char *path)
{
    struct stat buf;
    if (stat(path, &buf)) return 0;
    return (size_t)buf.st_size;
}

inline bool find_executable(const char *name)
{
    const size_t name_len = strlen(name);
    const char *environment = getenv("PATH");
    if (!environment) return false;
    const size_t dirs_len = strlen(environment);
    char *dirs = (char *)xrealloc(NULL, dirs_len + 1);
    if (!dirs) return false;
    strcpy(dirs, environment);
    bool res = false;
    const char *end = dirs + dirs_len + 1;
    for (char *dir = dirs, *q; !res && dir != end; dir = q) {
        for (q = dir; *q && *q != ':'; q++) assert(q + 1 < end);
        *q++ = 0;
        const size_t path_len = (q - dir) + name_len;
        char *path = (char *)xrealloc(NULL, path_len + 1);
        if (!path) {
            free(dirs);
            return false;
        }
        sprintf(path, "%s/%s", dir, name);
        assert(strlen(path) == path_len);
        res = file_readable(path);
        free(path);
    }
    free(dirs);
    return res;
}

static int bz2sig[] = { 0x42, 0x5A, 0x68, EOF };
static int gzsig[] = { 0x1F, 0x8B, EOF };
static int lzmasig[] = { 0x5D, 0x00, 0x00, 0x80, 0x00, EOF };
static int sig7z[] = { 0x37, 0x7A, 0xBC, 0xAF, 0x27, 0x1C, EOF };
static int xzsig[] = { 0xFD, 0x37, 0x7A, 0x58, 0x5A, 0x00, 0x00, EOF };

inline bool match_signature(const char *path, const int *sig)
{
    assert(path);
    FILE *tmp = fopen(path, "r");
    if (!tmp) return false;
    bool res = true;
    for (const int *p = sig; res && (*p != EOF); p++) res = (getc(tmp) == *p);
    fclose(tmp);
    return res;
}

inline FILE *open_pipe(const char *fmt, const char *path, const char *mode)
{
    size_t name_len = 0;
    while (fmt[name_len] && fmt[name_len] != ' ') name_len++;
    char *name = (char *)xrealloc(NULL, name_len + 1);
    if (!name) return 0;
    strncpy(name, fmt, name_len);
    name[name_len] = 0;
    bool found = find_executable(name);
    if (!found) fprintf(stderr, "c WARNING: failed to find executable %s\n", name);
    free(name);
    if (!found) return 0;
    char *cmd = (char *)xrealloc(NULL, strlen(fmt) + strlen(path) + 1);
    if (!cmd) return 0;
    sprintf(cmd, fmt, path);
    FILE *res = popen(cmd, mode);
    free(cmd);
    return res;
}

inline FILE *read_pipe(const char *fmt, const int *sig, const char *path)
{
    if (!file_readable(path)) return 0;
    if (sig && !match_signature(path, sig)) return 0;
    return open_pipe(fmt, path, "r");
}

inline FILE *open_to_read_file(const char *path)
{
    FILE *file = NULL;
#define READ_PIPE(SUFFIX, CMD, SIG)                                                                                    \
    do {                                                                                                               \
        if (has_suffix(path, SUFFIX)) {                                                                                \
            file = read_pipe(CMD, SIG, path);                                                                          \
        }                                                                                                              \
    } while (0)

    READ_PIPE(".bz2", "bzip2 -c -d %s", bz2sig);
    if (file) return file;
    READ_PIPE(".gz", "gzip -c -d %s", gzsig);
    if (file) return file;
    READ_PIPE(".lzma", "lzma -c -d %s", lzmasig);
    if (file) return file;
    READ_PIPE(".7z", "7z x -so %s 2>/dev/null", sig7z);
    if (file) return file;
    READ_PIPE(".xz", "xz -c -d %s", xzsig);
    if (file) return file;

    file = fopen(path, "r");
    return file;
}

//-------------------------------------------------------------------------------------------------
// End-of-file detection functions for StreamBuffer and char*:


static inline bool isEof(StreamBuffer &in) { return *in == EOF; }
static inline bool isEof(const char *in) { return *in == '\0'; }

//-------------------------------------------------------------------------------------------------
// Generic parse functions parametrized over the input-stream type.


template <class B> static void skipWhitespace(B &in)
{
    while ((*in >= 9 && *in <= 13) || *in == 32) ++in;
}


template <class B> static void skipLine(B &in)
{
    for (;;) {
        if (isEof(in)) return;
        if (*in == '\n') {
            ++in;
            return;
        }
        ++in;
    }
}


template <class B> static int parseInt(B &in)
{
    int val = 0;
    bool neg = false;
    skipWhitespace(in);
    if (*in == '-')
        neg = true, ++in;
    else if (*in == '+')
        ++in;
    if (*in < '0' || *in > '9') fprintf(stderr, "PARSE ERROR! Unexpected char: %c\n", *in), exit(3);
    while (*in >= '0' && *in <= '9') val = val * 10 + (*in - '0'), ++in;
    return neg ? -val : val;
}


// String matching: in case of a match the input iterator will be advanced the corresponding
// number of characters.
template <class B> static bool match(B &in, const char *str)
{
    int i;
    for (i = 0; str[i] != '\0'; i++)
        if (in[i] != str[i]) return false;

    in += i;

    return true;
}

// String matching: consumes characters eagerly, but does not require random access iterator.
template <class B> static bool eagerMatch(B &in, const char *str)
{
    for (; *str != '\0'; ++str, ++in)
        if (*str != *in) return false;
    return true;
}


//=================================================================================================
} // namespace MERGESAT_NSPACE

#endif
