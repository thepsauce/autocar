#include "args.h"
#include "conf.h"
#include "file.h"

#include <bfd.h>
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
        DLOG("CC = %s\n", Config.cc);
        DLOG("C_FLAGS = (");
        for (size_t i = 0; i < Config.num_c_flags; i++) {
            if (i > 0) {
                DLOG(" ");
            }
            DLOG("%s", Config.c_flags[i]);
        }
        DLOG(")\nC_LIBS = (");
        for (size_t i = 0; i < Config.num_c_libs; i++) {
            if (i > 0) {
                DLOG(" ");
            }
            DLOG("%s", Config.c_libs[i]);
        }
        DLOG(")\nSOURCES = %s\n"
                "TESTS = %s\n"
                "BUILD = %s\n"
                "INTERVAL = %ld\n",
                Config.sources, Config.tests,
                Config.build, Config.interval);
    }

    LOG("up and running\n");

    while (1) {
        Files.num = 0;

        if (!(collect_sources(Config.sources) &&
                collect_tests(Config.tests) &&
                compile_files() &&
                run_tests_and_compile_binaries())) {
            DLOG("did not reach the end\n");
        }
        usleep(1000 * Config.interval);
        DLOG("\n");
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
}

