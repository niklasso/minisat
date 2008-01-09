#include "Options.h"

vec<GenOption*> GenOption::options;
const char*     GenOption::help_usage = NULL;

void parseOptions(int& argc, char** argv, bool strict)
{
    int i, j;
    for (i = j = 1; i < argc; i++)

        if (match(argv[i], "--help-verb"))
            printUsageAndExit(argc, argv, true);
        else if (match(argv[i], "-h") || match(argv[i], "--help"))
            printUsageAndExit(argc, argv);
        else {
            bool parsed_ok = false;
        
            for (int k = 0; !parsed_ok && k < GenOption::options.size(); k++)
                parsed_ok = GenOption::options[k]->parse(argv[i]);

            if (!parsed_ok)
                if (strict && match(argv[i], "-"))
                    fprintf(stderr, "ERROR! Unknown flag %s.\n", argv[i]);
                else
                    argv[j++] = argv[i];
        }

    argc -= (i - j);
}


void setUsageHelp(const char* str){ GenOption::help_usage = str; }
void printUsageAndExit(int argc, char** argv, bool verbose)
{
    fprintf(stderr, GenOption::help_usage, argv[0]);
    fprintf(stderr, "OPTIONS:\n\n");
    
    for (int i = 0; i < GenOption::options.size(); i++)
        GenOption::options[i]->help(verbose);

    fprintf(stderr, "\n");
    fprintf(stderr, "  -h, --help    Print help message.\n");
    fprintf(stderr, "  --help-verb   Print verbose help message.\n");
    fprintf(stderr, "\n");
    exit(0);
}

