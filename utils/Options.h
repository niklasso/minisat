#ifndef Options_h
#define Options_h

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif
#include <stdint.h>
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <cstring>

#include "Vec.h"
#include "ParseUtils.h"


//==================================================================================================
// Top-level option parse/help functions:


extern void parseOptions(int& argc, char** argv);
extern void helpOptions (bool verbose);


//==================================================================================================
// Generic Options is an abstract class that gives the interface for all options:


class GenOption
{
 protected:
    typedef vec<GenOption*> OptList;
    static  OptList options;
 public:
    virtual bool parse       (const char* str) = 0;
    virtual void help        (bool verbose = false) = 0;
    friend  void parseOptions(int& argc, char** argv);
    friend  void helpOptions (bool verbose);
};


//==================================================================================================
// Base class for all options. Contains only a name and description of the option:


class BaseOption : public GenOption
{
 protected:
    const char* name;
    const char* description;
 public:
    BaseOption(const char* n, const char* d) : name(n), description(d) { options.push(this); }
};


//==================================================================================================
// The parametrized class of options. All intresting instances are given as specializations below:


template<class T>
class Option{};


//==================================================================================================
// 'typeName' type function:


template<class T>
static const char* typeName();


template<>
static const char* typeName<float>(){ return "<float>"; }
template<>
static const char* typeName<double>(){ return "<double>"; }
template<>
static const char* typeName<int32_t>(){ return "<int32_t>"; }
template<>
static const char* typeName<int64_t>(){ return "<int64_t>"; }


//==================================================================================================
// 'intMin'/'intMax' type functions:


template<class T>
static T intMin();
template<class T>
static T intMax();


template<>
static int32_t intMin<int32_t>(){ return INT32_MIN; }
template<>
static int32_t intMax<int32_t>(){ return INT32_MAX; }
template<>
static int64_t intMin<int64_t>(){ return INT64_MIN; }
template<>
static int64_t intMax<int64_t>(){ return INT64_MAX; }


//==================================================================================================
// Range classes with specialization for floating types:


template<class Num>
struct Range {
    Num begin;
    Num end;
    Range(Num b, Num e) : begin(b), end(e) {}
};


template<class Float>
struct FloatRange {
    Float begin;
    Float end;
    bool  begin_inclusive;
    bool  end_inclusive;
    FloatRange(Float b, bool binc, Float e, bool einc) : begin(b), end(e), begin_inclusive(binc), end_inclusive(einc) {}
};


template<>
struct Range<float>  : public FloatRange<float> { Range(float b, bool binc, float e, bool einc)   : FloatRange<float>(b, binc, e, einc) {}; };
template<>
struct Range<double> : public FloatRange<double>{ Range(double b, bool binc, double e, bool einc) : FloatRange<double>(b, binc, e, einc) {}; };


//==================================================================================================
// Float/Double options:


template<class Float>
class FloatOption : public BaseOption
{
 protected:
    Range<Float> range;
    Float        value;

 public:
    FloatOption(const char* n, const char* d, Range<Float> r, float def)
        : BaseOption(n, d), range(r), value(def) {
        // FIXME: set LC_NUMERIC to "C" to make sure that strtof/strtod parses decimal point correctly.
    }

    virtual bool parse(const char* str){
        const char* span = str; 

        if (!match(span, "-") || !match(span, name) || !match(span, "="))
            return false;

        char* end;
        Float tmp = strtod(span, &end);

        if (end == NULL) 
            return false;
        else if (tmp >= range.end && (!range.end_inclusive || tmp != range.end)){
            fprintf(stderr, "ERROR! value <%s> is too large for option \"%s\".\n", span, name);
            exit(1);
        }else if (tmp <= range.begin && (!range.begin_inclusive || tmp != range.begin)){
            fprintf(stderr, "ERROR! value <%s> is too small for option \"%s\".\n", span, name);
            exit(1); }

        value = tmp;

        return true;
    }

    virtual void help (bool verbose = false){
        fprintf(stderr, "  %-10s = %-8s  %c%4.2g .. %4.2g%c (default: %g)\n", 
                name, typeName<Float>(), 
                range.begin_inclusive ? '[' : '(', 
                range.begin,
                range.end,
                range.end_inclusive ? ']' : ')', 
                value);
        if (verbose){
            fprintf(stderr, "\n        %s\n", description);
            fprintf(stderr, "\n");
        }
    }
};


template<>
class Option<float>  : public FloatOption<float>
{
 public:
    Option(const char* n, const char* d, Range<float> r = Range<float>(-INFINITY, true, INFINITY, true), float def = float())
        : FloatOption<float>(n, d, r, def) {}

    operator float          (void) const  { return value; }
    Option<float>& operator=(float x)     { value = x; return *this; }
};


template<>
class Option<double> : public FloatOption<double>
{
 public:
    Option(const char* n, const char* d, Range<double> r = Range<double>(-INFINITY, true, INFINITY, true), double def = double())
        : FloatOption<double>(n, d, r, def) {}

    operator double          (void) const { return value; }
    Option<double>& operator=(double x)   { value = x; return *this; }
};


//==================================================================================================
// Int options:


template<class Int>
class IntOption : public BaseOption
{
 protected:
    Range<Int> range;
    Int        value;

 public:
    IntOption(const char* n, const char* d, Range<Int> r, Int def) 
        : BaseOption(n, d), range(r), value(def) {}
 
    virtual bool parse(const char* str){
        const char* span = str; 

        if (!match(span, "-") || !match(span, name) || !match(span, "="))
            return false;

        char* end;
        Int   tmp = strtoll(span, &end, 10);

        if (end == NULL) 
            return false;
        else if (tmp > range.end){
            fprintf(stderr, "ERROR! value <%s> is too large for option \"%s\".\n", span, name);
            exit(1);
        }else if (tmp < range.begin){
            fprintf(stderr, "ERROR! value <%s> is too small for option \"%s\".\n", span, name);
            exit(1); }

        value = tmp;

        return true;
    }

    virtual void help (bool verbose = false){
        fprintf(stderr, "  %-10s = %-8s [", name, typeName<Int>());
        if (range.begin == intMin<Int>())
            fprintf(stderr, "imin");
        else
            fprintf(stderr, "%4"PRIi64, (int64_t)range.begin);

        fprintf(stderr, " .. ");
        if (range.end == intMax<Int>())
            fprintf(stderr, "imax");
        else
            fprintf(stderr, "%4"PRIi64, (int64_t)range.end);

        fprintf(stderr, "] (default: %"PRIi64")\n", (int64_t)value);
        if (verbose){
            fprintf(stderr, "\n        %s\n", description);
            fprintf(stderr, "\n");
        }
    }
};


template<>
class Option<int32_t> : public IntOption<int32_t>
{
 public:
    Option(const char* n, const char* d, Range<int32_t> r = Range<int32_t>(INT32_MIN, INT32_MAX), int32_t def = int32_t())
        : IntOption<int32_t>(n, d, r, def) {};

    operator int32_t          (void) const { return value; }
    Option<int32_t>& operator=(int32_t x)  { value = x; return *this; }
};


template<>
class Option<int64_t> : public IntOption<int64_t>
{
 public:
    Option(const char* n, const char* d, Range<int64_t> r = Range<int64_t>(INT64_MIN, INT64_MAX), int64_t def = int64_t())
        : IntOption<int64_t>(n, d, r, def) {};

    operator int64_t          (void) const { return value; }
    Option<int64_t>& operator=(int64_t x)  { value = x; return *this; }
};


//==================================================================================================
// String option:


template<>
class Option<const char*> : public BaseOption
{
    const char* value;
 public:
    Option(const char* n, const char* d, const char* def = NULL) : BaseOption(n, d), value(def) {}

    operator const char*          (void) const     { return value; }
    Option<const char*>& operator=(const char* x)  { value = x; return *this; }

    virtual bool parse(const char* str){
        const char* span = str; 

        if (!match(span, "-") || !match(span, name) || !match(span, "="))
            return false;

        value = span;
        return true;
    }

    virtual void help (bool verbose = false){
        fprintf(stderr, "  %-10s = <string>\n", name);
        if (verbose){
            fprintf(stderr, "\n        %s\n", description);
            fprintf(stderr, "\n");
        }
    }    
};


//==================================================================================================
// Dummy option:


class DummyOption : public GenOption
{
 public:
    DummyOption() { options.push(this); }
    virtual bool parse(const char* str)     { return false; }
    virtual void help (bool verbose = false){ fprintf(stderr, "\n"); }
};


//==================================================================================================
// Bool option:


template<>
class Option<bool> : public BaseOption
{
    bool value;
 public:
    Option(const char* n, const char* d, bool v) : BaseOption(n, d), value(v) {}

    operator bool          (void) const { return value; }
    Option<bool>& operator=(bool b)     { value = b; return *this; }

    virtual bool parse(const char* str){
        const char* span = str; 

        if (match(span, "-") && match(span, name) && strlen(str) == strlen(name) + 1)
            value = true;
        else if (match(span, "-no") && match(span, name) && strlen(str) == strlen(name) + 3)
            value = false;
        else
            return false;

        return true;
    }

    virtual void help (bool verbose = false){

        fprintf(stderr, "  -%s, -no-%s", name, name);


        for (uint32_t i = 0; i < 30 - strlen(name)*2; i++)
            fprintf(stderr, " ");
        fprintf(stderr, " ");
        fprintf(stderr, "(default: %s)\n", value ? "on" : "off");
        if (verbose){
            fprintf(stderr, "\n        %s\n", description);
            fprintf(stderr, "\n");
        }
    }
};


#endif
