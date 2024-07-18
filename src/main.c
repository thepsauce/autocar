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

    if (Args.str_interval != NULL) {
        Args.interval = strtoll(Args.str_interval, NULL, 0);
        if (Args.interval < 0) {
            printf("invalid interval value\n");
            return 1;
        }
    }

    set_default_conf();

    if (!Args.no_config) {
        conf = Args.config == NULL ? "autocar.conf" : Args.config;
        if (!find_autocar_conf(conf) || !source_conf(conf)) {
            return 1;
        }
        if (Args.verbose) {
            dump_conf(stderr);
        }
    }

    LOG("up and running\n");

    pthread_mutex_init(&Files.lock, NULL);

    if (Args.interval > 0) {
        run_cli();
    }

    while (CliRunning) {
        if (!CliWantsPause) {
            if (check_conf() != 0) {
                usleep(1000 * 1000);
                continue;
            }
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
        if (Args.interval == 0) {
            break;
        }
        usleep(Args.interval);
    }

    /* free resources */
    for (size_t i = 0; i < Files.num; i++) {
        file = Files.ptr[i];
        free(file->path);
        free(file);
    }
    free(Files.ptr);

    clear_conf();
    return 0;
}
