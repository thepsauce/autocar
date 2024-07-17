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

const char *Commands[] = {
    [CMD_ADD] = "add",
    [CMD_CONFIG] = "config",
    [CMD_DELETE] = "delete",
    [CMD_ECHO] = "echo",
    [CMD_HELP] = "help",
    [CMD_LIST] = "list",
    [CMD_PAUSE] = "pause",
    [CMD_RUN] = "run",
    [CMD_SOURCE] = "source",
    [CMD_QUIT] = "quit",
};

int run_command(int cmd, char **args, size_t num_args, FILE *out)
{
    static const char *ext_strings[EXT_TYPE_MAX] = {
        [EXT_TYPE_SOURCE] = "source",
        [EXT_TYPE_HEADER] = "header",
        [EXT_TYPE_OBJECT] = "object",
        [EXT_TYPE_EXECUTABLE] = "executable",
        [EXT_TYPE_FOLDER] = "folder",
        [EXT_TYPE_OTHER] = "other"
    };

    int result = 0;

    glob_t g;
    struct file *file;
    int flags = 0;

    size_t index;
    char **exe_args;

    char str_flags[8];

    switch (cmd) {
    case CMD_ADD:
        pthread_mutex_lock(&Files.lock);
        for (size_t i = 0; i < num_args; i++) {
            if (strcmp(args[i], "-t") == 0) {
                flags |= FLAG_IS_TEST;
                continue;
            } else if (strcmp(args[i], "-r") == 0) {
                flags |= FLAG_IS_RECURSIVE;
                continue;
            }
            switch (glob(args[i], GLOB_TILDE | GLOB_BRACE, NULL, &g)) {
            case 0:
                for (size_t p = 0; p < g.gl_pathc; p++) {
                    add_file(g.gl_pathv[p], -1, flags);
                }
                globfree(&g);
                break;
            case GLOB_NOMATCH:
                printf("no matches found for: %s\n", args[i]);
                result = -1;
                break;
            default:
                printf("glob() error\n");
                result = -1;
                break;
            }
        }
        pthread_mutex_unlock(&Files.lock);
        break;

    case CMD_CONFIG:
        dump_conf(out);
        break;

    case CMD_DELETE:
        pthread_mutex_lock(&Files.lock);
        for (size_t i = 0; i < num_args; i++) {
            for (size_t f = 0; f < Files.num; ) {
                file = Files.ptr[f];
                if (fnmatch(args[i], file->path, 0) == 0) {
                    free(file->path);
                    free(file);
                    Files.num--;
                    memmove(&Files.ptr[f], &Files.ptr[f + 1],
                            sizeof(*Files.ptr) * (Files.num - f));
                } else {
                    f++;
                }
            }
        }
        pthread_mutex_unlock(&Files.lock);
        break;

    case CMD_ECHO:
        for (size_t i = 0; i < num_args; i++) {
            if (i > 0) {
                putchar(' ');
            }
            printf("%s", args[i]);
        }
        printf("\n");
        break;

    case CMD_HELP:
        printf("available commands:\n"
                "note: A file refers to a regular file or directory.\n"
                "      <files> accepts glob patterns\n"
                "  add [files] [-t files] - add files to the file list\n"
                "  build <files> - manually rebuild given files\n"
                "  delete <files> - delete files from the file list\n"
                "  help - show this help\n"
                "  list - list all files in the file list\n"
                "  pause - un-/pause the builder\n"
                "  run <index> - run\n"
                "  quit - quit the program\n");
        break;

    case CMD_LIST:
        pthread_mutex_lock(&Files.lock);
        for (size_t i = 0, f; i < Files.num; i++) {
            file = Files.ptr[i];
            f = 0;
            if (file->flags & FLAG_EXISTS) {
                str_flags[f++] = 'e';
            }
            if (file->flags & FLAG_HAS_MAIN) {
                str_flags[f++] = 'm';
            }
            if (file->flags & FLAG_IS_TEST) {
                str_flags[f++] = 't';
            }
            if (file->flags & FLAG_IS_RECURSIVE) {
                str_flags[f++] = 'r';
            }
            str_flags[f] = '\0';
            printf("(%zu) %s [%s] %s\n", i + 1,
                    file->path, ext_strings[file->type], str_flags);
        }
        pthread_mutex_unlock(&Files.lock);
        break;

    case CMD_PAUSE:
        CliWantsPause = !CliWantsPause;
        break;

    case CMD_QUIT:
        CliRunning = false;
        break;

    case CMD_RUN:
        if (num_args == 0) {
            printf("choose an executable:\n");
            pthread_mutex_lock(&Files.lock);
            for (size_t i = 0; i < Files.num; i++) {
                file = Files.ptr[i];
                if (file->type != EXT_TYPE_EXECUTABLE) {
                    continue;
                }
                printf("(%zu) %s\n", i + 1, file->path);
                index++;
            }
            pthread_mutex_unlock(&Files.lock);
        } else {
            pthread_mutex_lock(&Files.lock);
            file = search_file(args[0], NULL);
            if (file == NULL) {
                printf("'%s' does not exist\n", args[0]);
                pthread_mutex_unlock(&Files.lock);
                break;
            }
            if (file->type != EXT_TYPE_EXECUTABLE) {
                printf("'%s' is not an executable\n", file->path);
                pthread_mutex_unlock(&Files.lock);
                break;
            }
            exe_args = sreallocarray(NULL, num_args + 1, sizeof(*exe_args));
            for (size_t i = 0; i < num_args; i++) {
                exe_args[i] = args[i];
            }
            exe_args[num_args] = NULL;
            result = run_executable(exe_args, NULL, NULL);
            free(exe_args);
            pthread_mutex_unlock(&Files.lock);
        }
        break;

    case CMD_SOURCE:
        for (size_t i = 0; i < num_args; i++) {
            switch (glob(args[i], GLOB_TILDE | GLOB_BRACE, NULL, &g)) {
            case 0:
                for (size_t p = 0; p < g.gl_pathc; p++) {
                    if (source_path(g.gl_pathv[p]) != 0) {
                        printf("failed sourcing: '%s'\n", g.gl_pathv[p]);
                        break;
                    }
                }
                globfree(&g);
                break;
            case GLOB_NOMATCH:
                printf("no matches found for: '%s'\n", args[i]);
                result = -1;
                break;
            default:
                printf("glob() error\n");
                result = -1;
                break;
            }
        }
        break;
    }
    return result;
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
