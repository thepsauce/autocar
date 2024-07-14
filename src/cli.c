#include "cli.h"
#include "file.h"
#include "macros.h"
#include "util.h"

#include <stdlib.h>
#include <stdio.h>

#include <glob.h>

#include <readline/readline.h>
#include <readline/history.h>

volatile bool CliRunning;
volatile bool CliWantsPause;
pthread_mutex_t CliLock;

#define CMD_ADD     0
#define CMD_LIST    1
#define CMD_PAUSE   2
#define CMD_RUN     3
#define CMD_SELECT  4
#define CMD_TEST    5
#define CMD_QUIT    6

static const char *Commands[] = {
    [CMD_ADD] = "add",
    [CMD_LIST] = "list",
    [CMD_PAUSE] = "pause",
    [CMD_RUN] = "run",
    [CMD_SELECT] = "select",
    [CMD_TEST] = "test",
    [CMD_QUIT] = "quit",
};

static void run_command(int cmd, char **args, size_t num_args)
{
    glob_t g;

    switch (cmd) {
    case CMD_ADD:
        for (size_t i = 0; i < num_args; i++) {
            switch (glob(args[i], 0, NULL, &g)) {
            case 0:
                for (size_t p = 0; p < g.gl_pathc; p++) {
                    add_file(g.gl_pathv[p], 0, 0);
                }
                globfree(&g);
                break;
            case GLOB_NOMATCH:
                fprintf(stdout, "no matches found\n");
                break;
            default:
                fprintf(stdout, "glob() error\n");
                break;
            }
        }
        break;
    case CMD_PAUSE:
        CliWantsPause = true;
        break;
    case CMD_RUN:
        break;
    case CMD_SELECT:
        break;
    case CMD_TEST:
        break;
    case CMD_QUIT:
        CliRunning = false;
        break;
    }
}

static void read_line(void)
{
    char **args;
    size_t num_args;
    char *line;
    int cmd;
    size_t pref;

    line = readline("> ");
    if (line == NULL) {
        return;
    }

    split_string_at_space(line, &args, &num_args);

    if (num_args != 0) {
        for (cmd = 0; cmd < (int) ARRAY_SIZE(Commands); cmd++) {
            for (pref = 0; args[0][pref] != '\0'; pref++) {
                if (Commands[cmd][pref] != args[0][pref]) {
                    break;
                }
            }
            if (args[0][pref] == '\0') {
                break;
            }
        }
        if (cmd == (int) ARRAY_SIZE(Commands)) {
            fprintf(stderr, "command '%s' not found\n", args[0]);
        } else {
            pthread_mutex_lock(&CliLock);
            run_command(cmd, &args[1], num_args - 1);
            pthread_mutex_unlock(&CliLock);
        }
    }

    free(args);
    free(line);
}

void *cli_thread(void *unused)
{
    while (CliRunning) {
        read_line();
    }
    return unused;
}

bool run_cli(void)
{
    pthread_t thread_id;

    if (pthread_create(&thread_id, NULL, cli_thread, NULL) != 0) {
        return false;
    }
    pthread_mutex_init(&CliLock, NULL);
    CliRunning = true;
    return true;
}
