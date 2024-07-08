#include "args.h"
#include "conf.h"
#include "file.h"

#include <unistd.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (!parse_args(argc, argv)) {
        return 1;
    }

    if (Args.needs_help) {
        usage(stdout, argv[0]);
        return 1;
    }

    if (!Args.no_config) {
        const char *const conf = Args.config == NULL ? "autocar.conf" :
            Args.config;
        if (!find_autocar_config(conf) ||
                !source_config(conf) ||
                !check_config()) {
            return 1;
        }
        fprintf(stderr, "CC = %s\n", Config.cc);
        fprintf(stderr, "C_FLAGS = (");
        for (size_t i = 0; i < Config.num_c_flags; i++) {
            if (i > 0) {
                fprintf(stderr, " ");
            }
            fprintf(stderr, "%s", Config.c_flags[i]);
        }
        fprintf(stderr, ")\nC_LIBS = (");
        for (size_t i = 0; i < Config.num_c_libs; i++) {
            if (i > 0) {
                fprintf(stderr, " ");
            }
            fprintf(stderr, "%s", Config.c_libs[i]);
        }
        fprintf(stderr,
                ")\nSOURCES = %s\n"
                "TESTS = %s\n"
                "BUILD = %s\n"
                "INTERVAL = %ld\n",
                Config.sources, Config.tests,
                Config.build, Config.interval);
    }

    if (!collect_sources(Config.sources)) {
        return 1;
    }
    if (!collect_tests(Config.tests)) {
        return 1;
    }

    if (!compile_files()) {
        return 1;
    }

    if (!run_tests()) {
        return 1;
    }

    /* free resources */
    for (size_t i = 0; i < Files.num; i++) {
        free(Files.ptr[i].path);
    }
    free(Files.ptr);

    free(Config.cc);

    for (size_t i = 0; i < Config.num_c_flags; i++) {
        free(Config.c_flags[i]);
    }
    free(Config.c_flags);

    for (size_t i = 0; i < Config.num_c_libs; i++) {
        free(Config.c_libs[i]);
    }
    free(Config.c_libs);

    free(Config.sources);
    free(Config.tests);
    free(Config.build);

    return 0;
    while (1) {
        usleep(1000 * Config.interval);
    }

    return 0;
}

