#ifndef ARGS_H
#define ARGS_H

#define PACKAGE "autocar"
#define PACKAGE_VERSION "0.0.1"

#include <stdbool.h>
#include <stdio.h>

extern struct program_arguments {
    bool needs_help;
    bool verbose;
    char *verbosity;
    bool allow_parent_paths;
    char *config;
    bool no_config;
    char **files;
    size_t num_files;
} Args;

bool parse_args(int argc, char **argv);
void usage(FILE *fp, const char *programName);

#define DLOG(s, ...) do { \
    if (Args.verbosity != NULL && Args.verbosity[0] == 'd') { \
        fprintf(stderr, "%s:%d: ", __FILE__, __LINE__); \
        fprintf(stderr, (s), ##__VA_ARGS__); \
    } \
} while (0)

#define LOG(s, ...) do { \
    if (Args.verbosity != NULL && Args.verbosity[0] == 'd') { \
        fprintf(stderr, "%s:%d: ", __FILE__, __LINE__); \
        fprintf(stderr, (s), ##__VA_ARGS__); \
    } else if (Args.verbose) { \
        fprintf(stderr, s, ##__VA_ARGS__); \
    } \
} while (0)

#endif

