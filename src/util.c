#include "args.h"
#include "salloc.h"
#include "util.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/wait.h>

char *get_relative_path(const char *path)
{
    const char *orig_path;
    char *cwd;
    size_t cwd_len;
    char *s;
    size_t index = 0;
    size_t pref_cwd = 0, pref_path = 0;
    size_t num_slashes = 0;
    const char *seg;
    size_t move;

    orig_path = path;

    cwd = getcwd(NULL, 0);
    if (cwd == NULL) {
        fprintf(stderr, "getcwd: %s\n", strerror(errno));
        exit(1);
    }
    cwd_len = strlen(cwd);

    if (path[0] == '/') {
        while (cwd[pref_cwd] == path[pref_path]) {
            /* collapse multiple '/////' into a single one */
            if (path[pref_path] == '/') {
                do {
                    pref_path++;
                } while (path[pref_path] == '/');
            } else {
                pref_path++;
            }
            pref_cwd++;
        }

        if (cwd[pref_cwd] != '\0' ||
                (path[pref_path] != '/' &&
                 path[pref_path] != '\0')) {
            /* position right at the previous slash */
            while (pref_cwd > 0 && cwd[pref_cwd] != '/') {
                pref_cwd--;
                pref_path--;
            }
        }
        pref_path++;

        for (size_t i = pref_cwd; i < cwd_len; i++) {
            if (cwd[i] == '/') {
                num_slashes++;
            }
        }
    }
    /* else the path is already relative to the current path */

    /* allocate a good estimate of bytes, it can not be longer than that */
    s = smalloc(strlen(path) + 1);
    for (path += pref_path;;) {
        while (path[0] == '/') {
            path++;
        }
        seg = path;
        while (path[0] != '/' && path[0] != '\0') {
            path++;
        }

        if (seg == path) {
            break;
        }

        if (seg[0] == '.') {
            if (seg[1] == '.') {
                /* move up the path */
                if (seg[2] == '/' || seg[2] == '\0') {
                    if (index == 0) {
                        num_slashes++;
                    } else {
                        for (index--; index > 0 && s[--index] != '/'; ) {
                            (void) 0;
                        }
                    }
                    continue;
                }
            } else if (seg[1] == '/' || seg[1] == '\0') {
                /* simply ignore . */
                continue;
            }
        }
        if (index > 0) {
            s[index++] = '/';
        }
        memcpy(&s[index], seg, path - seg);
        index += path - seg;
    }

    if (!Args.allow_parent_paths && num_slashes > 0) {
        fprintf(stderr, "'%s': path is not allowed to be"
                " in a parent directory\n", orig_path);
        free(s);
        return NULL;
    }

    move = index;
    index += 3 * num_slashes;
    /* make sure when only going up that the final '/' is omitted */
    if (index > 0 && index == 3 * num_slashes) {
        index--;
    }
    s = srealloc(s, index + 1);
    memmove(&s[3 * num_slashes], &s[0], move);
    for (size_t i = 0; i < num_slashes; i++) {
        s[i * 3] = '.';
        s[i * 3 + 1] = '.';
        if (i * 3 + 2 != index) {
            s[i * 3 + 2] = '/';
        }
    }

    if (index == 0) {
        s = srealloc(s, 2);
        s[index++] = '.';
    }

    s[index] = '\0';

    free(cwd);
    return s;
}

void split_string_at_space(char *str, char ***psplit, size_t *pnum)
{
    char *s;
    char **split = NULL;
    size_t num = 0;

    while (1) {
        while (isspace(str[0])) {
            str++;
        }
        s = str;
        while (!isspace(str[0]) && str[0] != '\0') {
            str++;
        }
        if (s == str) {
            break;
        }

        if (str[0] != '\0') {
            str[0] = '\0';
            str++;
        }

        split = sreallocarray(split, num + 1, sizeof(*split));
        split[num++] = s;
    }

    *psplit = split;
    *pnum = num;
}

bool run_executable(char **args, const char *output_redirect,
        const char *input_redirect)
{
    int pid;
    int wstatus;

    for (char **a = args; a[0] != NULL; a++) {
        LOG("%s ", a[0]);
    }
    LOG("\n");

    pid = fork();
    if (pid == -1) {
        LOG("fork: %s\n", strerror(errno));
        return false;
    }
    if (pid == 0) {
        if (output_redirect != NULL) {
            if (freopen(output_redirect, "wb", stdout) == NULL) {
                LOG("freopen '%s' stdout: %s\n", output_redirect, strerror(errno));
                return false;
            }
        } else {
            dup2(STDOUT_FILENO, STDERR_FILENO);
        }
        if (input_redirect != NULL) {
            printf("%s\n", input_redirect);
            if (freopen(input_redirect, "rb", stdin) == NULL) {
                LOG("freopen '%s' stdin: %s\n", input_redirect, strerror(errno));
                return false;
            }
        }
        if (execvp(args[0], args) < 0) {
            LOG("execvp: %s\n", strerror(errno));
            return false;
        }
    } else {
        waitpid(pid, &wstatus, 0);
        if (WEXITSTATUS(wstatus) != 0) {
            LOG("`%s` returned: %d\n", args[0], wstatus);
            return false;
        }
    }
    return true;
}
