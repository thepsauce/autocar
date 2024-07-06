#ifndef ARGS_H
#define ARGS_H

#include <stdbool.h>
#include <stdio.h>

extern struct program_arguments {
    bool needs_help;
    bool do_rebuild;
    bool do_execute;
    char *test;
    bool do_debug;
    bool do_auto;
    char *build;
    char **sources;
    size_t num_sources;
    char **files;
    size_t num_files;
    char *gcc_flags;
} Args;

bool parse_args(int argc, char **argv);
void usage(FILE *fp, const char *programName);

#endif

