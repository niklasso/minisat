/***************************************************************************************[Options.h]
Copyright (c) 2008-2010, Niklas Sorensson

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

#ifndef MergeSat_Options_h
#define MergeSat_Options_h

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mtl/IntTypes.h"
#include "mtl/Sort.h"
#include "mtl/Vec.h"
#include "utils/ParseUtils.h"

#include <sstream>
#include <vector>

namespace MERGESAT_NSPACE
{

//==================================================================================================
// Top-level option parse/help functions:


bool parseOptions(int &argc, char **argv, bool strict = false);
void printUsageAndExit(int argc, char **argv, bool verbose = false);
void setUsageHelp(const char *str);
void setHelpPrefixStr(const char *str);

// print global options in PCS file format into the given File
void printOptions(FILE *pcsFile, int granularity = 0);
/// print option dependencies in PCS file format into the given File, add #NoAutoT to the description or category if an option should be excluded
void printOptionsDependencies(FILE *pcsFile, int granularity = 0);

//==================================================================================================
// Options is an abstract class that gives the interface for all types options:


class Option
{
    protected:
    const char *name;
    const char *description;
    const char *category;
    const char *type_name;

    Option *dependOnNonDefaultOf; // option that activates the current option

    static vec<Option *> &getOptionList()
    {
        static vec<Option *> options;
        return options;
    }
    static const char *&getUsageString()
    {
        static const char *usage_str;
        return usage_str;
    }
    static const char *&getHelpPrefixString()
    {
        static const char *help_prefix_str = "";
        return help_prefix_str;
    }

    struct OptionLt {
        bool operator()(const Option *x, const Option *y)
        {
            int test1 = strcmp(x->category, y->category);
            return test1 < 0 || (test1 == 0 && strcmp(x->type_name, y->type_name) < 0);
        }
    };

    Option(const char *name_,
           const char *desc_,
           const char *cate_,
           const char *type_,
           bool tunable = false,
           Option *dependOn = 0)
      : // push to this list, if present!
      name(name_)
      , description(desc_)
      , category(cate_)
      , type_name(type_)
      , dependOnNonDefaultOf(dependOn)
    {
        getOptionList().push(this);
    }

    public:
    virtual ~Option() {}

    virtual bool parse(const char *str) = 0;
    virtual void help(bool verbose = false) = 0;
    virtual void giveRndValue(std::string &optionText) = 0; // return a valid option-specification as it could appear on the command line

    virtual bool hasDefaultValue() = 0; // check whether the current value corresponds to the default value of the option
    virtual void printOptionCall(std::stringstream &strean) = 0; // print the call that is required to obtain that this option is set

    virtual void printOptions(FILE *pcsFile, int granularity = 0) = 0; // print the options specification

    virtual void reset() = 0;

    int getDependencyLevel() // return the number of options this option depends on (tree-like)
    {
        if (dependOnNonDefaultOf == 0) {
            return 0;
        }
        return dependOnNonDefaultOf->getDependencyLevel() + 1;
    }

    bool isEnabled()
    {
        if (dependOnNonDefaultOf == 0) {
            return true;
        } else {
            return !dependOnNonDefaultOf->hasDefaultValue() && dependOnNonDefaultOf->isEnabled();
        }
    }

    virtual bool canPrintOppositeOfDefault(int granularity) = 0; // represent whether printing the opposite value of the default value is feasible (only for bool and int with small domains)

    /// print the option (besides debug, can be overwritten by specific types, useful for strings)
    virtual bool wouldPrintOption() const
    {
        return description != 0 && 0 == strstr(description, "#NoAutoT") && 0 == strstr(category, "#NoAutoT");
    }

    void printOptionsDependencies(FILE *pcsFile, int granularity)
    {

        if (dependOnNonDefaultOf == 0) {
            return;
        } // no dependency
        if (!dependOnNonDefaultOf->canPrintOppositeOfDefault(granularity)) {
            return;
        } // cannot express opposite value of dependency-parent
        if (strstr(name, "debug") != 0 || strstr(description, "debug") != 0) {
            return;
        } // do not print the parameter, if its is related to debug output
        if (strstr(dependOnNonDefaultOf->name, "debug") != 0 || strstr(dependOnNonDefaultOf->description, "debug") != 0) {
            return;
        } // do not print the parameter, if its parent is related to debug output
        if (!wouldPrintOption()) {
            return;
        } // do not print this option
        if (!dependOnNonDefaultOf->wouldPrintOption()) {
            return;
        } // do not print parent option

        char defaultAsString[2048]; // take care of HUGEVAL in double, or string values
        dependOnNonDefaultOf->getNonDefaultString(granularity, defaultAsString, 2047);
        assert(strlen(defaultAsString) < 2047 && "memory overflow");
        fprintf(pcsFile, "%s   | %s in {%s} #  only active, if %s has not its default value \n", name,
                dependOnNonDefaultOf->name, defaultAsString, dependOnNonDefaultOf->name);
    }

    virtual void getNonDefaultString(int granularity, char *buffer, size_t size) = 0; // convert the default value into a string

    friend bool parseOptions(int &argc, char **argv, bool strict);
    friend void printUsageAndExit(int argc, char **argv, bool verbose);
    friend void setUsageHelp(const char *str);
    friend void setHelpPrefixStr(const char *str);

    friend void printOptions(FILE *pcsFile, int granularity);
    friend void printOptionsDependencies(FILE *pcsFile, int granularity);
};


//==================================================================================================
// Range classes with specialization for floating types:


struct IntRange {
    int begin;
    int end;
    IntRange(int b, int e) : begin(b), end(e) {}
};

struct Int64Range {
    int64_t begin;
    int64_t end;
    Int64Range(int64_t b, int64_t e) : begin(b), end(e) {}
};

struct DoubleRange {
    double begin;
    double end;
    bool begin_inclusive;
    bool end_inclusive;
    DoubleRange(double b, bool binc, double e, bool einc) : begin(b), end(e), begin_inclusive(binc), end_inclusive(einc)
    {
    }
};


//==================================================================================================
// Double options:


class DoubleOption : public Option
{
    protected:
    DoubleRange range;
    double value;
    double defaultValue; // the value that is given to this option during construction

    public:
    DoubleOption(const char *c,
                 const char *n,
                 const char *d,
                 double def = double(),
                 DoubleRange r = DoubleRange(-HUGE_VAL, false, HUGE_VAL, false),
                 bool tunable = false,
                 Option *dependOn = 0)
      : Option(n, d, c, "<double>", tunable, dependOn), range(r), value(def), defaultValue(def)
    {
        // FIXME: set LC_NUMERIC to "C" to make sure that strtof/strtod parses decimal point correctly.
    }

    operator double(void) const { return value; }
    operator double &(void) { return value; }
    DoubleOption &operator=(double x)
    {
        value = x;
        return *this;
    }

    virtual bool hasDefaultValue() { return value == defaultValue; }
    virtual void printOptionCall(std::stringstream &s) { s << "-" << name << "=" << value; }

    virtual void reset() { value = defaultValue; }

    virtual bool parse(const char *str)
    {
        const char *span = str;

        if (!match(span, "-") || !match(span, name) || !match(span, "=")) {
            return false;
        }

        char *end;
        double tmp = strtod(span, &end);

        if (end == NULL)
            return false;
        else if (tmp >= range.end && (!range.end_inclusive || tmp != range.end)) {
            fprintf(stderr, "ERROR! value <%s> is too large for option \"%s\".\n", span, name);
            exit(1);
        } else if (tmp <= range.begin && (!range.begin_inclusive || tmp != range.begin)) {
            fprintf(stderr, "ERROR! value <%s> is too small for option \"%s\".\n", span, name);
            exit(1);
        }

        value = tmp;
        // fprintf(stderr, "READ VALUE: %g\n", value);

        return true;
    }

    virtual void help(bool verbose = false)
    {
        fprintf(stderr, "  -%-12s = %-8s %c%4.2g .. %4.2g%c (default: %g)\n", name, type_name,
                range.begin_inclusive ? '[' : '(', range.begin, range.end, range.end_inclusive ? ']' : ')', value);
        if (verbose) {
            fprintf(stderr, "\n        %s\n", description);
            fprintf(stderr, "\n");
        }
    }

    void printOptions(FILE *pcsFile, int granularity)
    {
        if (strstr(name, "debug") != 0 || strstr(description, "debug") != 0) {
            return;
        } // do not print the parameter, if its related to debug output
        if (!wouldPrintOption()) {
            return;
        } // do not print option?

        // print only, if there is a default
        // choose between logarithmic scale and linear scale based on the number of elements in the list - more than 16 elements means it should be log (simple heuristic)
        double badd = 0, esub = 0;
        if (!range.begin_inclusive) {
            badd = 0.0001;
        }
        if (!range.end_inclusive) {
            esub = 0.0001;
        }
        // always logarithmic
        double endValue = range.end == HUGE_VAL ? (defaultValue > 1000000.0 ? defaultValue : 1000000.0) : range.end - esub;
        if (granularity == 0) { // use interval
            if (range.begin + badd > 0 || range.end - esub < 0) {
                fprintf(pcsFile, "%s  [%lf,%lf] [%lf]l   # %s\n", name, range.begin + badd, endValue, value, description);
            } else {
                fprintf(pcsFile, "%s  [%lf,%lf] [%lf]    # %s\n", name, range.begin + badd, endValue, value, description);
            }
        } else {                             // print linear distributed sampling for double option
            fprintf(pcsFile, "%s  {", name); // print name
            bool hitDefault = false;
            bool hitValue = false;
            if (granularity > 1) {
                double diff = (endValue - (range.begin + badd)) / (double)(granularity - 1);
                for (double v = range.begin + badd; v <= endValue; v += diff) {
                    if (v != range.begin + badd) {
                        fprintf(pcsFile, ",");
                    }                             // print comma, if there will be more and we printed one item already
                    fprintf(pcsFile, "%.4lf", v); // print current value
                    if (round(v * 10000) == round(defaultValue * 10000)) {
                        hitDefault = true;
                    } // otherwise we run into precision problems
                    if (round(v * 10000) == round(value * 10000)) {
                        hitValue = true;
                    } // otherwise we run into precision problems
                }
            }
            if (!hitValue) {
                fprintf(pcsFile, ",%.4lf", value);
            } // print current value of option as well!
            if (!hitDefault && round(defaultValue * 10000) != round(value * 10000)) {
                fprintf(pcsFile, ",%.4lf", defaultValue);
            } // print default value of option as well!
            fprintf(pcsFile, "} [%.4lf]    # %s\n", value, description);
        }
    }

    virtual bool canPrintOppositeOfDefault(int granularity) { return false; }

    virtual void getNonDefaultString(int granularity, char *buffer, size_t size)
    {
        // snprintf(buffer, size, "%lf", defaultValue); // could only print the default value
        return;
    }

    void giveRndValue(std::string &optionText)
    {
        double rndV = range.begin_inclusive ? range.begin : range.begin + 0.000001;
        rndV += rand();
        while (rndV > range.end) {
            rndV -= range.end - range.begin;
        }
        std::ostringstream strs;
        strs << rndV;
        optionText = "-" + optionText + "=" + strs.str();
    }
};


//==================================================================================================
// Int options:


class IntOption : public Option
{
    protected:
    IntRange range;
    int32_t value;
    int32_t defaultValue;

    public:
    IntOption(const char *c,
              const char *n,
              const char *d,
              int32_t def = int32_t(),
              IntRange r = IntRange(INT32_MIN, INT32_MAX),
              bool tunable = false,
              Option *dependOn = 0)
      : Option(n, d, c, "<int32>", tunable, dependOn), range(r), value(def), defaultValue(def)
    {
    }

    operator int32_t(void) const { return value; }
    operator int32_t &(void) { return value; }
    IntOption &operator=(int32_t x)
    {
        value = x;
        return *this;
    }

    virtual bool hasDefaultValue() { return value == defaultValue; }
    virtual void printOptionCall(std::stringstream &s) { s << "-" << name << "=" << value; }

    virtual void reset() { value = defaultValue; }

    virtual bool parse(const char *str)
    {
        const char *span = str;

        if (!match(span, "-") || !match(span, name) || !match(span, "=")) {
            return false;
        }

        char *end;
        int32_t tmp = strtol(span, &end, 10);

        if (end == NULL)
            return false;
        else if (tmp > range.end) {
            fprintf(stderr, "ERROR! value <%s> is too large for option \"%s\".\n", span, name);
            exit(1);
        } else if (tmp < range.begin) {
            fprintf(stderr, "ERROR! value <%s> is too small for option \"%s\".\n", span, name);
            exit(1);
        }

        value = tmp;

        return true;
    }

    virtual void help(bool verbose = false)
    {
        fprintf(stderr, "  -%-12s = %-8s [", name, type_name);
        if (range.begin == INT32_MIN)
            fprintf(stderr, "imin");
        else
            fprintf(stderr, "%4d", range.begin);

        fprintf(stderr, " .. ");
        if (range.end == INT32_MAX)
            fprintf(stderr, "imax");
        else
            fprintf(stderr, "%4d", range.end);

        fprintf(stderr, "] (default: %d)\n", value);
        if (verbose) {
            fprintf(stderr, "\n        %s\n", description);
            fprintf(stderr, "\n");
        }
    }

    void fillGranularityDomain(int granularity, std::vector<int> &values)
    {
        values.resize(granularity);
        int addedValues = 0;
        values[addedValues++] = value;
        int dist = value < 16 ? 1 : (value < 16000 ? 64 : 512); // smarter way to initialize the initial diff value
        if (addedValues < granularity) {
            values[addedValues++] = defaultValue;
        }
        while (addedValues < granularity) {
            if (value + dist > value && value + dist <= range.end) {
                values[addedValues++] = value + dist;
            }
            if (addedValues < granularity && value - dist >= range.begin) {
                values[addedValues++] = value - dist;
            }
            dist = dist * 4;
            if (value - dist < value && value + dist > range.end && value - dist < range.begin) {
                break;
            } // stop if there cannot be more values!
        }
        values.resize(addedValues);
        sort(values);
        int j = 0;
        assert(values[0] >= range.begin && values[0] <= range.end && "stay in bound");
        for (int i = 1; i < addedValues; ++i) {
            if (values[i] != values[j]) {
                assert(values[i] >= range.begin && values[i] <= range.end && "stay in bound");
                values[++j] = values[i];
            }
        }
        j++;
        assert(addedValues > 0 && "there has to be at least one option");
        assert(j <= addedValues && j <= granularity && "collected values hae to stay in bounds");
        values.resize(j);
    }

    void printOptions(FILE *pcsFile, int granularity)
    {

        if (strstr(name, "debug") != 0 || strstr(description, "debug") != 0) {
            return;
        } // do not print the parameter, if its related to debug output
        if (!wouldPrintOption()) {
            return;
        } // do not print option?

        // print only, if there is a default
        // choose between logarithmic scale and linear scale based on the number of elements in the list - more than 16 elements means it should be log (simple heuristic)
        if (granularity != 0) {
            fprintf(pcsFile, "%s  {", name);
            std::vector<int> values;
            fillGranularityDomain(granularity, values);
            for (size_t i = 0; i < values.size(); ++i) {
                if (i != 0) {
                    fprintf(pcsFile, ",");
                }
                fprintf(pcsFile, "%d", values[i]);
            }
            fprintf(pcsFile, "} [%d]    # %s\n", value, description);
        } else {
            if ((range.end - range.begin <= 16 && range.end - range.begin > 0 && range.end != INT32_MAX) ||
                (range.begin <= 0 && range.end >= 0)) {
                if (range.end - range.begin <= 16 && range.end - range.begin > 0) { // print all values if the difference is really small
                    fprintf(pcsFile, "%s  {%d", name, range.begin);
                    for (int i = range.begin + 1; i <= range.end; ++i) {
                        fprintf(pcsFile, ",%d", i);
                    }
                    fprintf(pcsFile, "} [%d]    # %s\n", value, description);
                } else {
                    fprintf(pcsFile, "%s  [%d,%d] [%d]i    # %s\n", name, range.begin, range.end, value, description);
                }
            } else {
                fprintf(pcsFile, "%s  [%d,%d] [%d]il   # %s\n", name, range.begin, range.end, value, description);
            }
        }
    }

    virtual bool canPrintOppositeOfDefault(int granularity)
    {
        return granularity != 0 || ((range.end - range.begin <= 16) && range.end - range.begin > 1);
    }

    virtual void getNonDefaultString(int granularity, char *buffer, size_t size)
    {
        if (granularity != 0) {
            std::vector<int> values;
            fillGranularityDomain(granularity, values);
            for (size_t i = 0; i < values.size(); ++i) {
                if (values[i] == defaultValue) {
                    continue;
                }                                        // do not print default value
                snprintf(buffer, size, "%d", values[i]); // convert value
                const int sl = strlen(buffer);
                size = size - strlen(buffer) - 1; // store new size
                if (i + 1 < values.size() && values[i + 1] != defaultValue) {
                    buffer[sl] = ',';           // set separator
                    buffer[sl + 1] = 0;         // indicate end of buffer
                    buffer = &(buffer[sl + 1]); // move start pointer accordingly (len is one more now)
                }
            }
        } else if (range.end - range.begin <= 16 && range.end - range.begin > 1) {
            for (int i = range.begin; i <= range.end; ++i) {
                if (i == defaultValue) {
                    continue;
                }                                // do not print default value
                snprintf(buffer, size, "%d", i); // convert value
                const int sl = strlen(buffer);
                size = size - strlen(buffer) - 1; // store new size
                if (i != range.end && i + 1 != defaultValue) {
                    buffer[sl] = ',';           // set separator
                    buffer[sl + 1] = 0;         // indicate end of buffer
                    buffer = &(buffer[sl + 1]); // move start pointer accordingly (len is one more now)
                }
            }
        }
    }

    void giveRndValue(std::string &optionText)
    {
        int rndV = range.begin;
        rndV += rand();
        while (rndV > range.end) {
            rndV -= range.end - range.begin;
        }
        std::ostringstream strs;
        strs << rndV;
        optionText = "-" + optionText + "=" + strs.str();
    }
};


// Leave this out for visual C++ until Microsoft implements C99 and gets support for strtoll.
#ifndef _MSC_VER

class Int64Option : public Option
{
    protected:
    Int64Range range;
    int64_t value;
    int64_t defaultValue;

    public:
    Int64Option(const char *c,
                const char *n,
                const char *d,
                int64_t def = int64_t(),
                Int64Range r = Int64Range(INT64_MIN, INT64_MAX),
                bool tunable = false,
                Option *dependOn = 0)
      : Option(n, d, c, "<int64>", tunable, dependOn), range(r), value(def), defaultValue(def)
    {
    }

    operator int64_t(void) const { return value; }
    operator int64_t &(void) { return value; }
    Int64Option &operator=(int64_t x)
    {
        value = x;
        return *this;
    }

    virtual bool hasDefaultValue() { return value == defaultValue; }
    virtual void printOptionCall(std::stringstream &s) { s << "-" << name << "=" << value; }

    virtual void reset() { value = defaultValue; }

    virtual bool parse(const char *str)
    {
        const char *span = str;

        if (!match(span, "-") || !match(span, name) || !match(span, "=")) return false;

        char *end;
        int64_t tmp = strtoll(span, &end, 10);

        if (end == NULL)
            return false;
        else if (tmp > range.end) {
            fprintf(stderr, "ERROR! value <%s> is too large for option \"%s\".\n", span, name);
            exit(1);
        } else if (tmp < range.begin) {
            fprintf(stderr, "ERROR! value <%s> is too small for option \"%s\".\n", span, name);
            exit(1);
        }

        value = tmp;

        return true;
    }

    virtual void help(bool verbose = false)
    {
        fprintf(stderr, "  -%-12s = %-8s [", name, type_name);
        if (range.begin == INT64_MIN)
            fprintf(stderr, "imin");
        else
            fprintf(stderr, "%4" PRIi64, range.begin);

        fprintf(stderr, " .. ");
        if (range.end == INT64_MAX)
            fprintf(stderr, "imax");
        else
            fprintf(stderr, "%4" PRIi64, range.end);

        fprintf(stderr, "] (default: %" PRIi64 ")\n", value);
        if (verbose) {
            fprintf(stderr, "\n        %s\n", description);
            fprintf(stderr, "\n");
        }
    }

    void fillGranularityDomain(int granularity, std::vector<int64_t> &values)
    {
        values.resize(granularity);
        int addedValues = 0;
        values[addedValues++] = value;
        int64_t dist = value < 16 ? 1 : (value < 16000 ? 64 : 512); // smarter way to initialize the initial diff value
        if (addedValues < granularity) {
            values[addedValues++] = defaultValue;
        }
        while (addedValues < granularity) {
            if (value + dist > value && value + dist <= range.end) {
                values[addedValues++] = value + dist;
            }
            if (addedValues < granularity && value - dist >= range.begin) {
                values[addedValues++] = value - dist;
            }
            dist = dist * 4;
            if (value - dist < value && value + dist > range.end && value - dist < range.begin) {
                break;
            } // stop if there cannot be more values!
        }
        values.resize(addedValues);
        sort(values);
        int j = 0;
        assert(values[0] >= range.begin && values[0] <= range.end && "stay in bound");
        for (int i = 1; i < addedValues; ++i) {
            if (values[i] != values[j]) {
                assert(values[i] >= range.begin && values[i] <= range.end && "stay in bound");
                values[++j] = values[i];
            }
        }
        j++;
        assert(addedValues > 0 && "there has to be at least one option");
        assert(j <= addedValues && j <= granularity && "collected values hae to stay in bounds");
        values.resize(j);
    }

    void printOptions(FILE *pcsFile, int granularity)
    {

        if (strstr(name, "debug") != 0 || strstr(description, "debug") != 0) {
            return;
        } // do not print the parameter, if its related to debug output
        if (!wouldPrintOption()) {
            return;
        } // do not print option?

        // print only, if there is a default
        // choose between logarithmic scale and linear scale based on the number of elements in the list - more than 16 elements means it should be log (simple heuristic)

        if (granularity != 0) {
            fprintf(pcsFile, "%s  {", name);
            std::vector<int64_t> values;
            fillGranularityDomain(granularity, values);
            for (size_t i = 0; i < values.size(); ++i) {
                if (i != 0) {
                    fprintf(pcsFile, ",");
                }
                fprintf(pcsFile, "%ld", values[i]);
            }
            fprintf(pcsFile, "} [%ld]    # %s\n", value, description);
        } else {
            if ((range.end - range.begin <= 16 && range.end - range.begin > 0 && range.end != INT32_MAX) ||
                (range.begin <= 0 && range.end >= 0)) {
                if (range.end - range.begin <= 16 && range.end - range.begin > 0) { // print all values if the difference is really small
                    fprintf(pcsFile, "%s  {%ld", name, range.begin);
                    for (int64_t i = range.begin + 1; i <= range.end; ++i) {
                        fprintf(pcsFile, ",%ld", i);
                    }
                    fprintf(pcsFile, "} [%ld]    # %s\n", value, description);
                } else {
                    fprintf(pcsFile, "%s  [%ld,%ld] [%ld]i    # %s\n", name, range.begin, range.end, value, description);
                }
            } else {
                fprintf(pcsFile, "%s  [%ld,%ld] [%ld]il   # %s\n", name, range.begin, range.end, value, description);
            }
        }
    }

    virtual bool canPrintOppositeOfDefault(int granularity)
    {
        return granularity != 0 || ((range.end - range.begin <= 16) && range.end - range.begin > 1);
    }

    virtual void getNonDefaultString(int granularity, char *buffer, size_t size)
    {
        if (granularity != 0) {
            std::vector<int64_t> values;
            fillGranularityDomain(granularity, values);
            for (size_t i = 0; i < values.size(); ++i) {
                if (values[i] == defaultValue) {
                    continue;
                }                                         // do not print default value
                snprintf(buffer, size, "%ld", values[i]); // convert value
                const int sl = strlen(buffer);
                size = size - strlen(buffer) - 1; // store new size
                if (i + 1 < values.size() && values[i + 1] != defaultValue) {
                    buffer[sl] = ',';           // set separator
                    buffer[sl + 1] = 0;         // indicate end of buffer
                    buffer = &(buffer[sl + 1]); // move start pointer accordingly (len is one more now)
                }
            }
        } else if (range.end - range.begin <= 16 && range.end - range.begin > 1) {
            for (int64_t i = range.begin; i <= range.end; ++i) {
                if (i == defaultValue) {
                    continue;
                }                                 // do not print default value
                snprintf(buffer, size, "%ld", i); // convert value
                const int sl = strlen(buffer);
                size = size - strlen(buffer) - 1; // store new size
                if (i != range.end && i + 1 != defaultValue) {
                    buffer[sl] = ',';           // set separator
                    buffer[sl + 1] = 0;         // indicate end of buffer
                    buffer = &(buffer[sl + 1]); // move start pointer accordingly (len is one more now)
                }
            }
        }
    }

    void giveRndValue(std::string &optionText)
    {
        int64_t rndV = range.begin;
        rndV += rand();
        while (rndV > range.end) {
            rndV -= range.end - range.begin;
        }
        std::ostringstream strs;
        strs << rndV;
        optionText = "-" + optionText + "=" + strs.str();
    }
};
#endif

//==================================================================================================
// String option:


class StringOption : public Option
{
    std::string *value;
    const char *defaultValue;

    public:
    StringOption(const char *c, const char *n, const char *d, const char *def = NULL, bool tunable = false, Option *dependOn = 0)
      : Option(n, d, c, "<std::string>", tunable, dependOn), value(def == NULL ? NULL : new std::string(def)), defaultValue(def)
    {
    }

    ~StringOption()
    {
        if (value != NULL) {
            delete value;
        }
    }

    operator const char *(void)const { return value == NULL ? NULL : value->c_str(); }
    //     operator      const char*& (void)           { return value; }
    StringOption &operator=(const char *x)
    {
        if (value == NULL) {
            value = new std::string(x);
        } else {
            *value = std::string(x);
        }
        return *this;
    }

    bool is_empty() { return value == NULL || value->empty(); }

    virtual bool hasDefaultValue()
    {
        if (value == NULL) {
            return defaultValue == NULL;
        } else if (defaultValue == NULL) {
            return value == NULL;
        } else {
            return *value == std::string(defaultValue);
        }
    }
    virtual void printOptionCall(std::stringstream &s)
    {
        if (value != NULL) {
            s << "-" << name << "=" << *value;
        } else {
            s << "-" << name << "=\"\"";
        }
    }

    virtual void reset()
    {
        if (defaultValue == NULL) {
            if (value != NULL) {
                delete value;
                value = NULL;
            }
        } else {
            if (value != NULL) {
                *value = std::string(defaultValue);
            } else {
                value = new std::string(defaultValue);
            }
        }
    }

    virtual bool parse(const char *str)
    {
        const char *span = str;

        if (!match(span, "-") || !match(span, name) || !match(span, "=")) return false;

        if (value == NULL) {
            value = new std::string(span);
        } else {
            *value = std::string(span);
        }
        return true;
    }

    virtual void help(bool verbose = false)
    {
        fprintf(stderr, "  -%-10s = %8s\n", name, type_name);
        if (verbose) {
            fprintf(stderr, "\n        %s\n", description);
            fprintf(stderr, "\n");
        }
    }

    void printOptions(FILE *pcsFile, int granularity)
    {

        if (strstr(name, "debug") != 0 || strstr(description, "debug") != 0) {
            return;
        } // do not print the parameter, if its related to debug output
        if (!wouldPrintOption()) {
            return;
        } // do not print option?

        // print only, if there is a default
        if (defaultValue != 0) {
            fprintf(pcsFile, "%s  {\"\",%s} [%s]     # %s\n", name, defaultValue, value->c_str(), description);
        }
    }

    bool wouldPrintOption() const
    {
        if (defaultValue == 0) {
            return false;
        }                                  // print string option only, if it has a default value
        return Option::wouldPrintOption(); // if the string has an default value, still check the global criterion
    }

    virtual bool canPrintOppositeOfDefault(int granularity) { return false; }

    virtual void getNonDefaultString(int granularity, char *buffer, size_t size)
    {
        if (defaultValue == 0) {
            buffer[0] = 0;
        } else {
            assert(size > strlen(defaultValue));
            strncpy(buffer, defaultValue, size);
        }
    }

    void giveRndValue(std::string &optionText)
    {
        optionText = ""; // NOTE: this could be files or any other thing, so do not consider this (for now - for special strings, another way might be found...)
    }
};


//==================================================================================================
// Bool option:


class BoolOption : public Option
{
    bool value;
    bool defaultValue;

    public:
    BoolOption(const char *c, const char *n, const char *d, bool v, bool tunable = false, Option *dependOn = 0)
      : Option(n, d, c, "<bool>", tunable, dependOn), value(v), defaultValue(v)
    {
    }

    operator bool(void) const { return value; }
    operator bool &(void) { return value; }
    BoolOption &operator=(bool b)
    {
        value = b;
        return *this;
    }

    virtual bool hasDefaultValue() { return value == defaultValue; }
    virtual void printOptionCall(std::stringstream &s)
    {
        if (value) {
            s << "-" << name;
        } else {
            s << "-no-" << name;
        }
    }

    virtual void reset() { value = defaultValue; }

    virtual bool parse(const char *str)
    {
        const char *span = str;

        if (match(span, "-")) {
            bool b = !match(span, "no-");

            if (strcmp(span, name) == 0) {
                value = b;
                return true;
            }
        }

        return false;
    }

    virtual void help(bool verbose = false)
    {

        fprintf(stderr, "  -%s, -no-%s", name, name);

        for (uint32_t i = 0; i < 32 - strlen(name) * 2; i++) fprintf(stderr, " ");

        fprintf(stderr, " ");
        fprintf(stderr, "(default: %s)\n", value ? "on" : "off");
        if (verbose) {
            fprintf(stderr, "\n        %s\n", description);
            fprintf(stderr, "\n");
        }
    }

    void printOptions(FILE *pcsFile, int granularity)
    {

        if (strstr(name, "debug") != 0 || strstr(description, "debug") != 0) {
            return;
        } // do not print the parameter, if its related to debug output
        if (!wouldPrintOption()) {
            return;
        } // do not print option?

        fprintf(pcsFile, "%s  {yes,no} [%s]     # %s\n", name, value ? "yes" : "no", description);
    }

    virtual bool canPrintOppositeOfDefault(int granularity) { return true; }

    virtual void getNonDefaultString(int granularity, char *buffer, size_t size)
    {
        assert(size > 3 && "cannot print values otherwise");
        strncpy(buffer, !defaultValue ? "yes" : "no", size);
    }

    void giveRndValue(std::string &optionText)
    {
        int r = rand();
        if (r % 5 > 1) { // more likely to be enabled
            optionText = "-" + std::string(name);
        } else {
            optionText = "-no-" + std::string(name);
        }
    }
};

//=================================================================================================
} // namespace MERGESAT_NSPACE

#endif
