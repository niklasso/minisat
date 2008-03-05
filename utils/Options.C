#include "mtl/Sort.h"
#include "utils/Options.h"

using namespace Minisat;

void Minisat::parseOptions(int& argc, char** argv, bool strict)
{
    int i, j;
    for (i = j = 1; i < argc; i++)

        if (strcmp(argv[i], "--help-verb") == 0)
            printUsageAndExit(argc, argv, true);
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
            printUsageAndExit(argc, argv);
        else {
            bool parsed_ok = false;
        
            for (int k = 0; !parsed_ok && k < Option::getOptionList().size(); k++){
                parsed_ok = Option::getOptionList()[k]->parse(argv[i]);

                // fprintf(stderr, "checking %d: %s against flag <%s> (%s)\n", i, argv[i], Option::getOptionList()[k]->name, parsed_ok ? "ok" : "skip");
            }

            if (!parsed_ok)
                if (strict && match(argv[i], "-"))
                    fprintf(stderr, "ERROR! Unknown flag \"%s\". Use '-h' for help.\n", argv[i]), exit(1);
                else
                    argv[j++] = argv[i];
        }

    argc -= (i - j);
}


void Minisat::setUsageHelp      (const char* str){ Option::getUsageString() = str; }
void Minisat::printUsageAndExit (int argc, char** argv, bool verbose)
{
    const char* usage = Option::getUsageString();
    if (usage != NULL)
        fprintf(stderr, usage, argv[0]);

    sort(Option::getOptionList(), Option::OptionLt());

    const char* prev_cat  = NULL;
    const char* prev_type = NULL;

    for (int i = 0; i < Option::getOptionList().size(); i++){
        const char* cat  = Option::getOptionList()[i]->category;
        const char* type = Option::getOptionList()[i]->type_name;

        if (cat != prev_cat)
            fprintf(stderr, "\n%s OPTIONS:\n\n", cat);
        else if (type != prev_type)
            fprintf(stderr, "\n");

        Option::getOptionList()[i]->help(verbose);

        prev_cat  = Option::getOptionList()[i]->category;
        prev_type = Option::getOptionList()[i]->type_name;
    }

    fprintf(stderr, "\nHELP OPTIONS:\n\n");
    fprintf(stderr, "  -h, --help    Print help message.\n");
    fprintf(stderr, "  --help-verb   Print verbose help message.\n");
    fprintf(stderr, "\n");
    exit(0);
}

