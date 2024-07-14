#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>

/**
 * @brief Get the given path relative to the current directory (getcwd()).
 *
 * If the current path is "/home/auto//car/" and the input `path` is
 * "/home/auto/../blue/red" then this function returns "../../blue/red".
 * The resulting path is always a path that leads from the current working
 * directory to given `path`.
 * If `getcwd()` fails, the program exits.
 *
 * @return Allocated string.
 */
char *get_relative_path(const char *path);

/**
 * @brief Splits the given string into substrings.
 *
 * This function modifies given string and places null terminators at appropiate
 * places, for example: "hello   there, my name   is tom?" is split into:
 * [ "hello", "there,", "my", "name", "is", "tom?" ] and the string itself is
 * turned into: "hello\0  there,\0my\0name\0  is\0tom?". The caller only needs
 * to free the result stored into `psplit` as all sub elements point into `str`.
 */
void split_string_at_space(char *str, char ***psplit, size_t *pnum);

/**
 * @brief Runs executable at `args[0]` and passes `args` as program arguments.
 *
 * `input_redirect` is a file that can be used to replace `stdin` for the
 * program to run, same goes for `output_redirect` (replaces `stdout`).
 *
 * @param args Args to send to the program, `args[0]` is the program itself.
 * @param output_redirect Replaces `stdout`, may be `NULL` to not replace.
 * @param input_redirect Replaces `stdin`, may be `NULL` to not replace.
 *
 * @return Whether running was successful.
 */
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

#endif

