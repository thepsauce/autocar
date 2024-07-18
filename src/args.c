#include "args.h"
#include "macros.h"

#include <stdio.h>
#include <string.h>

struct program_arguments Args;

bool LogNewLine = true;

struct program_opt {
    const char *loong;
    const char *desc;
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
} ProgramOptions[] = {
    { "allow-parent-paths", "allows paths to be in a parent directory",
        'a', 0, .b = &Args.allow_parent_paths },
    { "help", "show this help",
        'h', 0, .b = &Args.needs_help },
    { "verbose", "[arg] enable verbose output (-vdebug for maximum verbosity)",
        'v', 2, .b = &Args.verbose, .s = &Args.verbosity },
    { "config", "<name> specify a config (default: autocar.conf)",
        'c', 1, .s = &Args.config },
    { "no-config", "start without any config; load default options",
        'n', 0, .b = &Args.no_config },
    { "interval", "set a repeat interval",
        'i', 1, .s = &Args.str_interval },
};

void usage(FILE *fp, const char *program_name)
{
    struct program_opt *opt;

    fprintf(fp, "C project builder:\n"
            "%s [options]\n"
            "options:\n", program_name);
    for (size_t i = 0; i < ARRAY_SIZE(ProgramOptions); i++) {
        opt = &ProgramOptions[i];
        if (opt->loong != NULL) {
            fprintf(fp, "--%s", opt->loong);
        }
        if (opt->shrt != '\0') {
            if (opt->loong != NULL) {
                fputc('|', fp);
            }
            fputc(opt->shrt, fp);
        }
        fprintf(fp, " - %s\n", opt->desc);
    }
}

bool parse_args(int argc, char **argv)
{
    char *arg;
    char **vals;
    int num_vals;
    int on;
    const struct program_opt *o;
    char *equ;

    argc--;
    argv++;
    char s_arg[2] = { '.', '\0' };
    for (int i = 0; i != argc; ) {
        arg = argv[i];
        vals = NULL;
        num_vals = 0;
        on = -1;

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

                for (size_t i = 0; i < ARRAY_SIZE(ProgramOptions); i++) {
                    if (strcmp(ProgramOptions[i].loong, arg) == 0) {
                        o = &ProgramOptions[i];
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
                        num_vals++;
                        if (on != 1) {
                            break;
                        }
                    }
                } else if (equ != NULL) {
                    num_vals = 1;
                }
            } else {
                s_arg[0] = *(arg++);
                for (size_t i = 0; i < ARRAY_SIZE(ProgramOptions); i++) {
                    if (ProgramOptions[i].shrt == s_arg[0]) {
                        o = &ProgramOptions[i];
                        on = o->n;
                        break;
                    }
                }
                if (arg[0] != '\0') {
                    if (on > 0) {
                        argv[i] = arg;
                        vals = &argv[i];
                        num_vals = 1;
                        i++;
                    } else {
                        arg[-1] = '-';
                        argv[i] = arg - 1;
                    }
                } else {
                    i++;
                    if (on == 1 && i != argc) {
                        vals = &argv[i++];
                        num_vals = 1;
                    } else if (on == 3) {
                        vals = &argv[i];
                        for (; i != argc && argv[i][0] != '-'; i++) {
                            num_vals++;
                        }
                    }
                }
                arg = s_arg;
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
            if (num_vals > 0) {
                fprintf(stderr, "option '%s' does not expect any arguments\n",
                        arg);
                return false;
            }
            *o->b = true;
            break;
        case 1:
            if (num_vals == 0) {
                fprintf(stderr, "option '%s' expects one argument\n", arg);
                return false;
            }
            *o->s = vals[0];
            break;
        case 2:
            *o->b = true;
            if (num_vals == 1) {
                *o->s = vals[0];
            }
            break;
        case 3:
            *o->v.p = vals;
            *o->v.n = num_vals;
            break;
        }
    }
    return true;
}
