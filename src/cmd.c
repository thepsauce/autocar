#include "args.h"
#include "cli.h"
#include "cmd.h"
#include "conf.h"
#include "file.h"
#include "macros.h"
#include "salloc.h"
#include "util.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <glob.h>
#include <fnmatch.h>

#include <unistd.h>

#include "cmd_add.h"
#include "cmd_build.h"
#include "cmd_config.h"
#include "cmd_delete.h"
#include "cmd_echo.h"
#include "cmd_help.h"
#include "cmd_generate.h"
#include "cmd_list.h"
#include "cmd_pause.h"
#include "cmd_quit.h"
#include "cmd_run.h"
#include "cmd_source.h"

const struct command Commands[] = {
    [CMD_ADD] = { "add", cmd_add, "[files] [-tr files]",
        "add files to the file list" },
    [CMD_BUILD] = { "build", cmd_build, "[--collect|-c]",
        "build everything" },
    [CMD_CONFIG] = { "config", cmd_config, "", "show all config options" },
    [CMD_DELETE] = { "delete", cmd_delete, "[files]",
        "deletes given files from the file list" },
    [CMD_ECHO] = { "echo", cmd_echo, "[args]",
        "prints the expandend arguments to stdout" },
    [CMD_HELP] = { "help", cmd_help, "[args]",
        "prints this help or only specific commands" },
    [CMD_GENERATE] = { "generate", cmd_generate, "<shell|make>",
        "generate a shell or make build file" },
    [CMD_LIST] = { "list", cmd_list, "", "list all files" },
    [CMD_PAUSE] = { "pause", cmd_pause, "", "un-/pause the buffer" },
    [CMD_RUN] = { "run", cmd_run, "[<name> [args]]",
        "run file with" "given name. Use `run` without any arguments\n"
        "  to list all main programs.\n"
        "  Use `run $<index> [args]` for convenience" },
    [CMD_SOURCE] = { "source", cmd_source, "[files]",
        "runs all given files as autocar script" },
    [CMD_QUIT] = { "quit", cmd_quit, "", "quit all" },
};

int run_command(int cmd, char **args, size_t num_args, FILE *out)
{
    return Commands[cmd].cmd_proc(args, num_args, out);
}

static char *get_shebang_cmd(FILE *fp, const char *cat)
{
    char *cmd = NULL;
    size_t n = 0;
    ssize_t l;

    l = getline(&cmd, &n, fp);
    if (l < 0) {
        if (errno == 0) {
            fprintf(stderr, "invalid shebang\n");
        } else {
            fprintf(stderr, "getline: %s\n", strerror(errno));
        }
        fclose(fp);
        return false;
    }
    if (cmd[l - 1] == '\n') {
        cmd[--l] = ' ';
    } else {
        cmd[l] = ' ';
    }
    fclose(fp);

    cmd = srealloc(cmd, l + 1 + strlen(cat) + 1);
    strcat(cmd, cat);
    return cmd;
}

int source_path(const char *path)
{
    int result;
    FILE *fp, *pp;
    char *cmd;

    DLOG("source: %s\n", path);

    fp = fopen(path, "r");
    if (fp == NULL) {
        fprintf(stderr, "fopen '%s': %s\n", path, strerror(errno));
        return -1;
    }

    if (fgetc(fp) == '#' && fgetc(fp) == '!') {
        cmd = get_shebang_cmd(fp, path);
        if (cmd == NULL) {
            return -1;
        }
        DLOG("has shebang: %s\n", cmd);
        pp = popen(cmd, "r");
        if (pp == NULL) {
            fprintf(stderr, "popen '%s': %s\n", cmd, strerror(errno));
            fclose(fp);
            free(cmd);
            return -1;
        }
        free(cmd);
        result = eval_file(pp);
        pclose(pp);
    } else {
        rewind(fp);
        result = eval_file(fp);
        fclose(fp);
    }
    return result;
}
