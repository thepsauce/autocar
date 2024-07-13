#include "args.h"
#include "conf.h"
#include "file.h"
#include "cli.h"

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
        DLOG("CC = %s\nC_FLAGS = (", Config.cc);
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
                "BUILD_SOURCE = %s\n"
                "BUILD_TEST = %s\n"
                "INTERVAL = %ld\n",
                Config.folders[FOLDER_SOURCE],
                Config.folders[FOLDER_TEST],
                Config.folders[FOLDER_BUILD],
                Config.folders[FOLDER_BUILD_SOURCE],
                Config.folders[FOLDER_BUILD_TEST],
                Config.interval);
    }

    LOG("up and running\n");

    run_cli();

    while (CliRunning) {
        if (!CliWantsPause) {
            pthread_mutex_lock(&CliLock);
            if (!collect_files()) {
                DLOG("0: did not reach the end\n");
            } else if (!build_objects()) {
                DLOG("1: did not reach the end\n");
            } else if (!link_executables()) {
                DLOG("2: did not reach the end\n");
            } else if (!run_tests()) {
                DLOG("3: did not reach the end\n");
            }
            pthread_mutex_unlock(&CliLock);
            fprintf(stderr, "==========================\n");
        }
        usleep(1000 * Config.interval);
    }

    /* free resources */
    for (size_t i = 0; i < Files.num; i++) {
        struct file *const file = Files.ptr[i];
        free(file->name);
        free(file->path);
        free(file);
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

    for (int i = 0; i < FOLDER_MAX; i++) {
        free(Config.folders[i]);
    }
    for (int i = 0; i < EXT_TYPE_MAX; i++) {
        free(Config.exts[i]);
    }
    return 0;
}
