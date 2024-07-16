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

#define CMD_ADD     0
#define CMD_BUILD   1
#define CMD_CONFIG  2
#define CMD_DELETE  3
#define CMD_EXECUTE 4
#define CMD_HELP    5
#define CMD_LIST    6
#define CMD_PAUSE   7
#define CMD_RUN     8
#define CMD_QUIT    9

static const char *Commands[] = {
    [CMD_ADD] = "add",
    [CMD_BUILD] = "build",
    [CMD_CONFIG] = "config",
    [CMD_DELETE] = "delete",
    [CMD_EXECUTE] = "execute",
    [CMD_HELP] = "help",
    [CMD_LIST] = "list",
    [CMD_PAUSE] = "pause",
    [CMD_RUN] = "run",
    [CMD_QUIT] = "quit",
};

static int run_command(int cmd, char **args, size_t num_args)
{
    static const char *ext_strings[EXT_TYPE_MAX] = {
        [EXT_TYPE_SOURCE] = "source",
        [EXT_TYPE_HEADER] = "header",
        [EXT_TYPE_OBJECT] = "object",
        [EXT_TYPE_EXECUTABLE] = "executable",
        [EXT_TYPE_FOLDER] = "folder",
        [EXT_TYPE_OTHER] = "other"
    };

    glob_t g;
    struct file *file;
    int result = 0;
    int flags = 0;

    switch (cmd) {
    case CMD_ADD:
        pthread_mutex_lock(&Files.lock);
        for (size_t i = 0; i < num_args; i++) {
            if (strcmp(args[i], "-t") == 0) {
                flags |= FLAG_IS_TEST;
                continue;
            }
            //else if (strcmp(args[i], "-r") == 0) {
            /* TODO: recursion */
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

    case CMD_BUILD:
        pthread_mutex_lock(&Files.lock);
        for (size_t i = 0; i < num_args; i++) {
            for (size_t f = 0; f < Files.num; f++) {
                file = Files.ptr[f];
                if (fnmatch(args[i], file->path, 0) == 0) {
                    if (file->type != EXT_TYPE_OBJECT) {
                        result = -1;
                        i = num_args - 1;
                        printf("can only rebuild objects but '%s' is not\n",
                                file->path);
                        break;
                    }
                }
            }
        }
        pthread_mutex_unlock(&Files.lock);
        break;

    case CMD_CONFIG:
        printf("PATH=%s\nCC = %s\nC_FLAGS = (", Config.path, Config.cc);
        for (size_t i = 0; i < Config.num_c_flags; i++) {
            if (i > 0) {
                printf(" ");
            }
            printf("%s", Config.c_flags[i]);
        }
        printf(")\nC_LIBS = (");
        for (size_t i = 0; i < Config.num_c_libs; i++) {
            if (i > 0) {
                DLOG(" ");
            }
            printf("%s", Config.c_libs[i]);
        }
        printf(")\nEXTS = (");
        for (size_t i = 0; i < EXT_TYPE_MAX; i++) {
            if (i > 0) {
                printf(" ");
            }
            printf("%s", Config.exts[i]);
        }
        printf(")\nBUILD = %s\n"
                "INTERVAL = %ld\n"
                "ERR_FILE = %s\n"
                "PROMPT = %s\n",
                Config.build,
                Config.interval,
                Config.err_file,
                Config.prompt);
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

    case CMD_EXECUTE:
        for (size_t i = 0; i < num_args; i++) {
            switch (glob(args[i], GLOB_TILDE | GLOB_BRACE, NULL, &g)) {
            case 0:
                for (size_t p = 0; p < g.gl_pathc; p++) {
                    if (source_file(g.gl_pathv[p]) != 0) {
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

    case CMD_PAUSE:
        CliWantsPause = !CliWantsPause;
        break;

    case CMD_RUN:
        if (num_args == 0) {
            printf("choose an executable:\n");
            pthread_mutex_lock(&Files.lock);
            for (size_t i = 0, index = 1; i < Files.num; i++) {
                struct file *const file = Files.ptr[i];
                if (file->type != EXT_TYPE_EXECUTABLE) {
                    continue;
                }
                printf("(%zu) %s\n", index, file->path);
                index++;
            }
            pthread_mutex_unlock(&Files.lock);
        } else {
            size_t index;
            char *exe_args[2];

            index = strtoull(args[0], NULL, 0);
            pthread_mutex_lock(&Files.lock);
            for (size_t i = 0; i < Files.num; i++) {
                struct file *const file = Files.ptr[i];
                if (file->type != EXT_TYPE_EXECUTABLE) {
                    continue;
                }
                index--;
                if (index == 0) {
                    exe_args[0] = file->path;
                    exe_args[1] = NULL;
                    result = run_executable(exe_args, NULL, NULL);
                    break;
                }
            }
            pthread_mutex_unlock(&Files.lock);
            if (index != 0) {
                printf("invalid index\n");
            }
        }
        break;

    case CMD_LIST:
        pthread_mutex_lock(&Files.lock);
        for (size_t i = 0; i < Files.num; i++) {
            struct file *const file = Files.ptr[i];
            char flags[8];
            int f = 0;
            if (file->flags & FLAG_EXISTS) {
                flags[f++] = 'e';
            }
            if (file->flags & FLAG_HAS_MAIN) {
                flags[f++] = 'm';
            }
            if (file->flags & FLAG_IS_TEST) {
                flags[f++] = 't';
            }
            flags[f] = '\0';
            printf("(%zu) %s [%s] %s\n", i + 1,
                    file->path, ext_strings[file->type], flags);
        }
        pthread_mutex_unlock(&Files.lock);
        break;

    case CMD_QUIT:
        CliRunning = false;
        break;
    }
    return result;
}

#define PARSE_STATE_REGULAR 0
#define PARSE_STATE_APPEND 1
#define PARSE_STATE_EQUAL 2

char *read_arg(const char **ps, int *pstate)
{
    const char *s;

    size_t a_arg = 10;
    char *arg;
    size_t len_arg = 0;

    bool esc = false;
    char quot = '\0';

    arg = smalloc(a_arg);

    s = *ps;
    while (isblank(s[0])) {
        s++;
    }
    for (; s[0] != '\0'; s++) {
        if (s[0] == ';' && !esc && quot == '\0') {
            break;
        }
        switch (s[0]) {
        case '\\':
            esc = !esc;
            if (esc) {
                continue;
            }
            break;
        case '\"':
        case '\'':
            if (!esc) {
                if (quot == s[0]) {
                    quot = '\0';
                } else {
                    quot = s[0];
                }
                continue;
            }
            break;
        case ' ':
        case '\t':
            if (!esc && quot == '\0') {
                goto reg;
            }
            break;
        case '+':
            if (*pstate != PARSE_STATE_REGULAR) {
                break;
            }
            if (!esc && quot == '\0') {
                if (s[1] == '=') {
                    *pstate = PARSE_STATE_APPEND;
                    s += 2;
                    goto ret;
                }
                break;
            }
            break;
        case '=':
            if (*pstate != PARSE_STATE_REGULAR) {
                break;
            }
            if (!esc && quot == '\0') {
                *pstate = PARSE_STATE_EQUAL;
                arg[len_arg] = '\0';
                s++;
                goto ret;
            }
            break;
        }
        if (len_arg + 2 > a_arg) {
            a_arg *= 2;
            arg = srealloc(arg, a_arg);
        }
        arg[len_arg++] = s[0];
        esc = false;
    }

reg:
    if (len_arg == 0) {
        free(arg);
        return NULL;
    }

ret:
    arg[len_arg] = '\0';
    *ps = s;
    return arg;
}

int run_command_line(const char *s)
{
    int result = 0;
    char *arg;
    char **args;
    size_t num_args;

    int state;

    int cmd;
    size_t pref;

    DLOG("running cmd: %s\n", s);

next_segment:
    state = PARSE_STATE_REGULAR;
    args = NULL;
    num_args = 0;

    while (arg = read_arg(&s, &state), arg != NULL) {
        if (arg[0] == '\0') {
            continue;
        }
        args = sreallocarray(args, num_args + 1, sizeof(*args));
        args[num_args++] = arg;
    }

    if (num_args == 0) {
        goto end;
    }

    DLOG("got args in state (%d):", state);
    for (size_t i = 0; i < num_args; i++) {
        DLOG(" %s", args[i]);
    }
    DLOG("\n");

    switch (state) {
    case PARSE_STATE_REGULAR:
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
            result = -1;
        } else {
            result = run_command(cmd, &args[1], num_args - 1);
        }
        break;
    case PARSE_STATE_EQUAL:
    case PARSE_STATE_APPEND:
        result = set_conf(args[0], &args[1], num_args - 1,
                state == PARSE_STATE_APPEND);
        switch (result) {
        case SET_CONF_SINGLE:
            printf("config variable '%s' expects a single argument\n",
                    args[0]);
            break;
        case SET_CONF_APPEND:
            printf("config variable '%s' can not be appended to\n",
                    args[0]);
            break;
        case SET_CONF_EXIST:
            printf("config variable '%s' does not exist\n",
                    args[0]);
            break;
        case -1:
            printf("unexpected error in `set_conf()`"
                    " while trying to set: '%s'\n",
                    args[0]);
            break;
        }
        break;
    }

    for (size_t i = 0; i < num_args; i++) {
        free(args[i]);
    }
    free(args);

end:
    while (isblank(s[0])) {
        s++;
    }
    if (s[0] == ';') {
        s++;
        if (result == 0) {
            goto next_segment;
        }
    }
    return result;
}

/**
 * Parses a config file line by line using `run_command_line()`.
 */
static int parse_file(FILE *fp)
{
    char *line = NULL;
    size_t a;
    ssize_t len;

    while ((len = getline(&line, &a, fp)) > 0) {
        if (line[len - 1] == '\n') {
            len--;
            line[len] = '\0';
        }
        if (run_command_line(line) != 0) {
            free(line);
            return false;
        }
    }
    free(line);
    if (errno != 0) {
        fprintf(stderr, "getline: %s\n", strerror(errno));
        return -1;
    }
    return 0;
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

int source_file(const char *path)
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
        result = parse_file(pp);
        pclose(pp);
    } else {
        rewind(fp);
        result = parse_file(fp);
        fclose(fp);
    }
    return result;
}
