#include "args.h"
#include "conf.h"
#include "file.h"
#include "cli.h"

#include <bfd.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    char *conf;
    struct file *file;

    if (!parse_args(argc, argv)) {
        return 1;
    }

    if (Args.needs_help) {
        usage(stdout, argv[0]);
        return 1;
    }

    if (!Args.no_config) {
        conf = Args.config == NULL ? "autocar.conf" : Args.config;
        if (!find_autocar_config(conf) ||
                !source_config(conf) ||
                !check_config()) {
            return 1;
        }
        DLOG("PATH=%s\nCC = %s\nC_FLAGS = (", Config.path, Config.cc);
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
        DLOG(")\nEXTS = (");
        for (size_t i = 0; i < EXT_TYPE_MAX; i++) {
            if (i > 0) {
                DLOG(" ");
            }
            DLOG("%s", Config.exts[i]);
        }
        DLOG(")\nBUILD = %s\n"
                "INTERVAL = %ld\n"
                "ERR_FILE = %s\n"
                "PROMPT = %s\n",
                Config.build,
                Config.interval,
                Config.err_file,
                Config.prompt);
    }

    LOG("up and running\n");

    pthread_mutex_init(&Files.lock, NULL);

    run_cli();

    while (CliRunning) {
        if (!CliWantsPause) {
            pthread_mutex_lock(&Files.lock);
            if (collect_files() != 0) {
                DLOG("0: did not reach the end\n");
            } else if (!build_objects()) {
                DLOG("1: did not reach the end\n");
            } else if (!link_executables()) {
                DLOG("2: did not reach the end\n");
            } else if (!run_tests()) {
                DLOG("3: did not reach the end\n");
            }
            pthread_mutex_unlock(&Files.lock);
        }
        usleep(1000 * Config.interval);
    }

    /* free resources */
    for (size_t i = 0; i < Files.num; i++) {
        file = Files.ptr[i];
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

    for (int i = 0; i < EXT_TYPE_FOLDER; i++) {
        free(Config.exts[i]);
    }

    free(Config.build);

    free(Config.err_file);

    free(Config.prompt);
    return 0;
}
