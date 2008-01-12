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


extern void parseOptions     (int& argc, char** argv, bool strict = false);
extern void printUsageAndExit(int  argc, char** argv, bool verbose = false);
extern void setUsageHelp     (const char* str);


//==================================================================================================
// Options is an abstract class that gives the interface for all types options:


class Option
{
 protected:

    const char* name;
    const char* description;
    const char* category;
    const char* type_name;

    static vec<Option*>& getOptionList () { static vec<Option*> options; return options; }
    static const char*&  getUsageString() { static const char* usage_str; return usage_str; }

    struct OptionLt {
        bool operator()(const Option* x, const Option* y) {
            int test1 = strcmp(x->category, y->category);
            return test1 < 0 || test1 == 0 && strcmp(x->type_name, y->type_name) < 0;
        }
    };

    Option(const char* name_, 
           const char* desc_,
           const char* cate_,
           const char* type_) : 
      name       (name_)
    , description(desc_)
    , category   (cate_)
    , type_name  (type_)
    { 
        getOptionList().push(this); 
    }

 public:
    virtual bool parse             (const char* str)      = 0;
    virtual void help              (bool verbose = false) = 0;

    friend  void parseOptions      (int& argc, char** argv, bool strict);
    friend  void printUsageAndExit (int  argc, char** argv, bool verbose);
    friend  void setUsageHelp      (const char* str);
};


//==================================================================================================
// Range classes with specialization for floating types:


struct IntRange {
    int64_t begin;
    int64_t end;
    IntRange(int64_t b, int64_t e) : begin(b), end(e) {}
};


struct DoubleRange {
    double begin;
    double end;
    bool  begin_inclusive;
    bool  end_inclusive;
    DoubleRange(double b, bool binc, double e, bool einc) : begin(b), end(e), begin_inclusive(binc), end_inclusive(einc) {}
};


//==================================================================================================
// Double options:


class DoubleOption : public Option
{
 protected:
    DoubleRange range;
    double      value;

 public:
    DoubleOption(const char* c, const char* n, const char* d, double def = double(), DoubleRange r = DoubleRange(-INFINITY, false, INFINITY, false))
        : Option(n, d, c, "<double>"), range(r), value(def) {
        // FIXME: set LC_NUMERIC to "C" to make sure that strtof/strtod parses decimal point correctly.
    }

    operator      double   (void) const { return value; }
    operator      double&  (void)       { return value; }
    DoubleOption& operator=(double x)   { value = x; return *this; }

    virtual bool parse(const char* str){
        const char* span = str; 

        if (!match(span, "-") || !match(span, name) || !match(span, "="))
            return false;

        char*  end;
        double tmp = strtod(span, &end);

        if (end == NULL) 
            return false;
        else if (tmp >= range.end && (!range.end_inclusive || tmp != range.end)){
            fprintf(stderr, "ERROR! value <%s> is too large for option \"%s\".\n", span, name);
            exit(1);
        }else if (tmp <= range.begin && (!range.begin_inclusive || tmp != range.begin)){
            fprintf(stderr, "ERROR! value <%s> is too small for option \"%s\".\n", span, name);
            exit(1); }

        value = tmp;
        fprintf(stderr, "READ VALUE: %g\n", value);

        return true;
    }

    virtual void help (bool verbose = false){
        fprintf(stderr, "  -%-10s = %-8s %c%4.2g .. %4.2g%c (default: %g)\n", 
                name, type_name, 
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


//==================================================================================================
// Int options:


class IntOption : public Option
{
 protected:
    IntRange range;
    int64_t  value;

 public:
    IntOption(const char* c, const char* n, const char* d, int64_t def = int64_t(), IntRange r = IntRange(INT64_MIN, INT64_MAX))
        : Option(n, d, c, "<int64>"), range(r), value(def) {}
 
    operator   int64_t   (void) const { return value; }
    operator   int64_t&  (void)       { return value; }
    IntOption& operator= (int64_t x)  { value = x; return *this; }

    virtual bool parse(const char* str){
        const char* span = str; 

        if (!match(span, "-") || !match(span, name) || !match(span, "="))
            return false;

        char*   end;
        int64_t tmp = strtoll(span, &end, 10);

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
        fprintf(stderr, "  -%-10s = %-8s [", name, type_name);
        if (range.begin == INT64_MIN)
            fprintf(stderr, "imin");
        else
            fprintf(stderr, "%4"PRIi64, range.begin);

        fprintf(stderr, " .. ");
        if (range.end == INT64_MAX)
            fprintf(stderr, "imax");
        else
            fprintf(stderr, "%4"PRIi64, range.end);

        fprintf(stderr, "] (default: %"PRIi64")\n", value);
        if (verbose){
            fprintf(stderr, "\n        %s\n", description);
            fprintf(stderr, "\n");
        }
    }
};


//==================================================================================================
// String option:


class StringOption : public Option
{
    const char* value;
 public:
    StringOption(const char* c, const char* n, const char* d, const char* def = NULL) 
        : Option(n, d, c, "<string>"), value(def) {}

    operator      const char*  (void) const     { return value; }
    operator      const char*& (void)           { return value; }
    StringOption& operator=    (const char* x)  { value = x; return *this; }

    virtual bool parse(const char* str){
        const char* span = str; 

        if (!match(span, "-") || !match(span, name) || !match(span, "="))
            return false;

        value = span;
        return true;
    }

    virtual void help (bool verbose = false){
        fprintf(stderr, "  -%-10s = %8s\n", name, type_name);
        if (verbose){
            fprintf(stderr, "\n        %s\n", description);
            fprintf(stderr, "\n");
        }
    }    
};


//==================================================================================================
// Bool option:


class BoolOption : public Option
{
    bool value;

 public:
    BoolOption(const char* c, const char* n, const char* d, bool v) 
        : Option(n, d, c, "<bool>"), value(v) {}

    operator    bool     (void) const { return value; }
    operator    bool&    (void)       { return value; }
    BoolOption& operator=(bool b)     { value = b; return *this; }

    virtual bool parse(const char* str){
        const char* span = str; 
        
        if (match(span, "-")){
            bool b = !match(span, "no-");

            if (strcmp(span, name) == 0){
                value = b;
                return true; }
        }

        return false;
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
