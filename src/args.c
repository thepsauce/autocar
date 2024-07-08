#include "args.h"
#include "macros.h"

#include <stdio.h>
#include <string.h>

struct program_arguments Args;

bool parse_args(int argc, char **argv)
{
    struct opt {
        const char *loong;
        char shrt;
        /* 0 - no arguments */
        /* 1 - single argument */
        /* 2 - arguments until the next '-' */
        int n;
        union {
            bool *b;
            struct {
                char ***p;
                size_t *n;
            } v;
            char **s;
        } dest;
    } pArgs[] = {
        { "help", 'h', 0, { .b = &Args.needs_help } },
        { "config", 'c', 1, { .s = &Args.config } },
        { "no-config", '\0', 0, { .b = &Args.no_config } },
        { "allow-parent-paths", '\0', 0, { .b = &Args.allow_parent_paths } },
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
                    } else if (on == 2) {
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
            if (o->dest.b != NULL) {
                *o->dest.b = true;
            }
            break;
        case 1:
            if (numVals == 0) {
                fprintf(stderr, "option '%s' expects one argument\n", arg);
                return false;
            }
            if (o->dest.s != NULL) {
                *o->dest.s = vals[0];
            }
            break;
        case 2:
            if (o->dest.v.p != NULL) {
                *o->dest.v.p = vals;
                *o->dest.v.n = numVals;
            }
            break;
        }
    }
    return true;
}

void usage(FILE *fp, const char *programName)
{
    fprintf(fp, "C project builder:\n"
            "%s [options]\n",
            programName);
}
