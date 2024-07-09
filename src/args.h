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

extern bool LogNewLine;

#define DLOG(s, ...) do { \
    const char *const _s = (s); \
    if (Args.verbosity != NULL && Args.verbosity[0] == 'd') { \
        if (LogNewLine) { \
            fprintf(stderr, "%s:%d: ", __FILE__, __LINE__); \
            LogNewLine = false; \
        } \
        fprintf(stderr, _s, ##__VA_ARGS__); \
    } \
    if (_s[strlen(_s) - 1] == '\n') { \
        LogNewLine = true; \
    } \
} while (0)

#define LOG(s, ...) do { \
    const char *const _s = (s); \
    if (Args.verbosity != NULL && Args.verbosity[0] == 'd') { \
        if (LogNewLine) { \
            fprintf(stderr, "%s:%d: ", __FILE__, __LINE__); \
            LogNewLine = false; \
        } \
        fprintf(stderr, _s, ##__VA_ARGS__); \
    } else if (Args.verbose) { \
        fprintf(stderr, _s, ##__VA_ARGS__); \
    } \
    if (_s[strlen(_s) - 1] == '\n') { \
        LogNewLine = true; \
    } \
} while (0)

#endif

