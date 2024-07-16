#include "cmd.h"
#include "cli.h"
#include "conf.h"
#include "file.h"
#include "macros.h"
#include "util.h"

#include <stdlib.h>
#include <stdio.h>

#include <signal.h>

#include <readline/readline.h>
#include <readline/history.h>

volatile bool CliRunning;
volatile bool CliWantsPause;
pthread_mutex_t CliLock;

static void signal_handler(int sig)
{
    (void) sig;
    fprintf(stderr, "received SIGINT\n");
}

static void read_line(void)
{
    char *line;

    line = readline(Config.prompt);
    if (line == NULL) {
        return;
    }
    run_command_line(line);
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

    signal(SIGINT, signal_handler);

    if (pthread_create(&thread_id, NULL, cli_thread, NULL) != 0) {
        return false;
    }
    pthread_mutex_init(&CliLock, NULL);
    CliRunning = true;
    return true;
}
