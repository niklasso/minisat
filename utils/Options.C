#include "Options.h"

GenOption::OptList GenOption::options;

void parseOptions(int& argc, char** argv){
    int i, j;
    for (i = j = 1; i < argc; i++)
        for (int k = 0; k < GenOption::options.size(); k++)
            if (!GenOption::options[k]->parse(argv[i]))
                argv[j++] = argv[i];
    argc -= (i - j);
}


void helpOptions(bool verbose)
{
    fprintf(stderr, "OPTIONS:\n\n");
    
    for (int i = 0; i < GenOption::options.size(); i++)
        GenOption::options[i]->help(verbose);
}

