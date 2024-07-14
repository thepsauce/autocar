#include "args.h"
#include "macros.h"

#include <stdio.h>
#include <string.h>

struct program_arguments Args;

bool LogNewLine = true;

bool parse_args(int argc, char **argv)
{
    struct opt {
        const char *loong;
        char shrt;
        /* 0 - no arguments */
        /* 1 - single argument */
        /* 2 - optional argument */
        /* 3 - arguments until the next '-' */
        int n;
        bool *b; /* 0 | 2 */
        struct {
            char ***p;
            size_t *n;
        } v; /* 3 */
        char **s; /* 1 */
    } pArgs[] = {
        { "help", 'h', 0, .b = &Args.needs_help },
        { "verbose", 'v', 2, .b = &Args.verbose, .s = &Args.verbosity },
        { "config", 'c', 1, .s = &Args.config },
        { "no-config", '\0', 0, .b = &Args.no_config },
        { "allow-parent-paths", '\0', 0, .b = &Args.allow_parent_paths },
    };

    argc--;
    argv++;
    char sArg[2] = { '.', '\0' };
    for (int i = 0; i != argc; ) {
        char *arg = argv[i];
        char **vals = NULL;
        int numVals = 0;
        const struct opt *o;
        int on = -1;
        char *equ;
        if (arg[0] == '-') {
            arg++;
            equ = strchr(arg, '=');
            if (equ != NULL) {
                *(equ++) = '\0';
            }
            if (arg[0] == '-' || equ != NULL) {
                if (arg[0] == '-') {
                    arg++;
                }

                if (arg[0] == '\0') {
                    i++;
                    Args.files = &argv[i];
                    Args.num_files = argc - i;
                    break;
                }

                for (size_t i = 0; i < ARRAY_SIZE(pArgs); i++) {
                    if (strcmp(pArgs[i].loong, arg) == 0) {
                        o = &pArgs[i];
                        on = o->n;
                        break;
                    }
                }
                if (equ != NULL) {
                    argv[i] = equ;
                } else {
                    i++;
                }

                vals = &argv[i];
                if (on > 0 && i != argc) {
                    for (; i != argc && argv[i][0] != '-'; i++) {
                        numVals++;
                        if (on != 1) {
                            break;
                        }
                    }
                } else if (equ != NULL) {
                    numVals = 1;
                }
            } else {
                sArg[0] = *(arg++);
                for (size_t i = 0; i < ARRAY_SIZE(pArgs); i++) {
                    if (pArgs[i].shrt == sArg[0]) {
                        o = &pArgs[i];
                        on = o->n;
                        break;
                    }
                }
                if (arg[0] != '\0') {
                    if (on > 0) {
                        argv[i] = arg;
                        vals = &argv[i];
                        numVals = 1;
                        i++;
                    } else {
                        arg[-1] = '-';
                        argv[i] = arg - 1;
                    }
                } else {
                    i++;
                    if (on == 1 && i != argc) {
                        vals = &argv[i++];
                        numVals = 1;
                    } else if (on == 3) {
                        vals = &argv[i];
                        for (; i != argc && argv[i][0] != '-'; i++) {
                            numVals++;
                        }
                    }
                }
                arg = sArg;
            }
        } else {
            /* use rest as files */
            Args.files = &argv[i];
            Args.num_files = argc - i;
            break;
        }
        switch (on) {
        case -1:
            fprintf(stderr, "invalid option '%s'\n", arg);
            return false;
        case 0:
            if (numVals > 0) {
                fprintf(stderr, "option '%s' does not expect any arguments\n",
                        arg);
                return false;
            }
            *o->b = true;
            break;
        case 1:
            if (numVals == 0) {
                fprintf(stderr, "option '%s' expects one argument\n", arg);
                return false;
            }
            *o->s = vals[0];
            break;
        case 2:
            *o->b = true;
            if (numVals == 1) {
                *o->s = vals[0];
            }
            break;
        case 3:
            *o->v.p = vals;
            *o->v.n = numVals;
            break;
        }
    }
    return true;
}

void usage(FILE *fp, const char *programName)
{
    fprintf(fp, "C project builder:\n"
            "%s [options]\n"
            "options:\n"
            "--help|-h show this help\n"
            "--verbose|-v [arg] enable verbose output (-vdebug for maximum verbosity)\n"
            "--config|-c <name> specify a config (default: autocar.conf)\n"
            "--no-config start without any config; load default options\n"
            "--allow-parent-paths allows paths to be in a parent directory\n" ,
            programName);
}
