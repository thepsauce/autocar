#ifndef ARGS_H
#define ARGS_H

#include <stdbool.h>
#include <stdio.h>

extern struct program_arguments {
    bool needs_help;
    bool allow_parent_paths;
    char *config;
    bool no_config;
    char **files;
    size_t num_files;
} Args;

bool parse_args(int argc, char **argv);
void usage(FILE *fp, const char *programName);

#endif

