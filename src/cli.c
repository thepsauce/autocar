#include "cli.h"
#include "file.h"
#include "macros.h"

#include <stdlib.h>
#include <stdio.h>

#include <glob.h>

#include <readline/readline.h>
#include <readline/history.h>

volatile bool CliRunning;
volatile bool CliWantsPause;
pthread_mutex_t CliLock;

#define CMD_ADD 0
#define CMD_PAUSE 1
#define CMD_RUN 2
#define CMD_QUIT 3

static const char *Commands[] = {
    [CMD_ADD] = "add",
    [CMD_PAUSE] = "pause",
    [CMD_RUN] = "run",
    [CMD_QUIT] = "quit",
};

static void run_command(int cmd, char **args, int num_args)
{
    glob_t g;

    switch (cmd) {
    case CMD_ADD:
        for (int i = 0; i < num_args; i++) {
            switch (glob(args[i], 0, NULL, &g)) {
            case 0:
                for (size_t p = 0; p < g.gl_pathc; p++) {
                    add_path(g.gl_pathv[p]);
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
        /* TODO: */
        break;
    case CMD_QUIT:
        CliRunning = false;
        break;
    }
}

static void read_line(void)
{
    char *args[8];
    int num_args = 0;
    char *line, *s, *e;
    int cmd;

    line = readline("> ");
    if (line == NULL) {
        return;
    }

    s = line;
    while (s[0] != '\0') {
        while (isspace(s[0])) {
            s++;
        }
        e = s;
        while (!isspace(e[0]) && e[0] != '\0') {
            e++;
        }
        if (s == e) {
            break;
        }
        if (num_args == ARRAY_SIZE(args)) {
            fprintf(stderr, "too many arguments provided\n");
            num_args = 0;
            break;
        }
        args[num_args++] = s;
        if (e[0] == '\0') {
            break;
        }
        e[0] = '\0';
        s = e + 1;
    }

    if (num_args != 0) {
        for (cmd = 0; cmd < (int) ARRAY_SIZE(Commands); cmd++) {
            if (args[0][strspn(Commands[cmd], args[0])] == '\0') {
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
